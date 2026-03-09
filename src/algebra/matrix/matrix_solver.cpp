//! Implements `MatrixSolver::solve`. Internal helpers (anonymous namespace)
//! cover: augmented-matrix construction, rank counting, Gaussian elimination
//! with partial pivoting, back-substitution, free-variable parameterisation,
//! and LU factorization (Doolittle with partial pivoting) with forward/back
//! solve.

#include "algebra/matrix/matrix_solver.hpp"
#include "core/tolerance.hpp"
#include "diagnostics/kinds/solver_errors.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace math_solver {
    namespace {

        using Matrix = std::vector<std::vector<double>>;

        /// Build the augmented matrix [A | b] from LinearForms and a variable
        /// order. Row i corresponds to forms[i]:
        ///   A[i][j] = forms[i].get_coeff(var_order[j])
        ///   b[i]    = -forms[i].constant
        Matrix build_augmented(const std::vector<LinearForm>&  forms,
                               const std::vector<std::string>& var_order) {
            const size_t m = forms.size();
            const size_t n = var_order.size();
            Matrix       mat(m, std::vector<double>(n + 1, 0.0));
            for (size_t i = 0; i < m; ++i) {
                for (size_t j = 0; j < n; ++j)
                    mat[i][j] = forms[i].get_coeff(var_order[j]);
                mat[i][n] = -forms[i].constant;
            }
            return mat;
        }

        /// Count the number of non-zero rows (in the first `col_limit`
        /// columns).
        int count_rank(const Matrix& mat, size_t col_limit) {
            int rank = 0;
            for (const auto& row : mat) {
                bool nonzero = false;
                for (size_t j = 0; j < col_limit && j < row.size(); ++j) {
                    if (std::abs(row[j]) > kPivotTol) {
                        nonzero = true;
                        break;
                    }
                }
                if (nonzero)
                    ++rank;
            }
            return rank;
        }

        /// Gaussian elimination with partial pivoting on an m×(n+1) augmented
        /// matrix. Modifies `mat` in-place.  Returns the smallest absolute
        /// pivot value seen.
        double gauss_eliminate(Matrix& mat, size_t n) {
            const size_t m         = mat.size();
            double       min_pivot = 1e18;
            size_t       pivot_row = 0;

            for (size_t col = 0; col < n && pivot_row < m; ++col) {
                // Partial pivoting: find the row with the largest absolute
                // value in this column at or below pivot_row.
                size_t max_row = pivot_row;
                double max_val = std::abs(mat[pivot_row][col]);
                for (size_t r = pivot_row + 1; r < m; ++r) {
                    if (std::abs(mat[r][col]) > max_val) {
                        max_val = std::abs(mat[r][col]);
                        max_row = r;
                    }
                }

                if (max_val < kEpsilon)
                    continue; // effectively zero column; skip

                if (max_row != pivot_row)
                    std::swap(mat[pivot_row], mat[max_row]);

                double pivot = mat[pivot_row][col];
                if (std::abs(pivot) < min_pivot)
                    min_pivot = std::abs(pivot);

                // Eliminate entries below the pivot.
                for (size_t r = pivot_row + 1; r < m; ++r) {
                    if (std::abs(mat[r][col]) < kEpsilon)
                        continue;
                    double factor = mat[r][col] / pivot;
                    for (size_t c = col; c <= n; ++c)
                        mat[r][c] -= factor * mat[pivot_row][c];
                    mat[r][col] = 0.0; // force exact zero
                }

                ++pivot_row;
            }
            return min_pivot;
        }

        /// Back-substitution on a row-echelon augmented matrix produced by
        /// gauss_eliminate().  Returns solutions in `x` (size n).
        /// Precondition: the system has a unique solution (rank_A == n).
        void back_substitute(const Matrix& mat, size_t n,
                             std::vector<double>& x) {
            const size_t m = mat.size();
            x.assign(n, 0.0);

            // Find pivot columns — scan each row for the leading non-zero
            // entry. pivot_col[row] = the variable column for that row's pivot.
            std::vector<int> pivot_col(m, -1);
            for (size_t r = 0; r < m; ++r) {
                for (size_t c = 0; c < n; ++c) {
                    if (std::abs(mat[r][c]) > kPivotTol) {
                        pivot_col[r] = static_cast<int>(c);
                        break;
                    }
                }
            }

            // Back-substitution from the last pivot upward.
            for (int r = static_cast<int>(m) - 1; r >= 0; --r) {
                if (pivot_col[r] < 0)
                    continue;
                size_t c   = static_cast<size_t>(pivot_col[r]);
                double rhs = mat[r][n];
                for (size_t k = c + 1; k < n; ++k)
                    rhs -= mat[r][k] * x[k];
                x[c] = rhs / mat[r][c];
            }
        }

        /// Parameterise free variables and express pivot variables in terms of
        /// them. Returns a map: var_name → expression string.
        std::map<std::string, std::string>
        parameterise_free(const Matrix&                   mat,
                          const std::vector<std::string>& var_order,
                          std::vector<std::string>&       free_var_names) {
            const size_t     n = var_order.size();
            const size_t     m = mat.size();

            // Identify pivot columns.
            std::vector<int> pivot_col(m, -1);
            for (size_t r = 0; r < m; ++r) {
                for (size_t c = 0; c < n; ++c) {
                    if (std::abs(mat[r][c]) > kPivotTol) {
                        pivot_col[r] = static_cast<int>(c);
                        break;
                    }
                }
            }

            std::set<size_t> pivot_set;
            for (size_t r = 0; r < m; ++r)
                if (pivot_col[r] >= 0)
                    pivot_set.insert(static_cast<size_t>(pivot_col[r]));

            // Free variables are those not in the pivot set.
            // Use the variable's own name as its parameter symbol so that
            // expressions read naturally (e.g. "x = 5 - y" not "x = 5 - t0").
            std::map<size_t, std::string> free_param; // col → parameter name
            for (size_t c = 0; c < n; ++c) {
                if (pivot_set.find(c) == pivot_set.end()) {
                    free_param[c] = var_order[c];
                    free_var_names.push_back(var_order[c]);
                }
            }

            // Build expressions for pivot variables.
            std::map<std::string, std::string> exprs;
            // First, assign free variables their parameter names.
            for (auto& [c, pname] : free_param)
                exprs[var_order[c]] = pname;

            // Back-substitute to express each pivot variable.
            for (int r = static_cast<int>(m) - 1; r >= 0; --r) {
                if (pivot_col[r] < 0)
                    continue;
                size_t                   c = static_cast<size_t>(pivot_col[r]);

                double                   rhs = mat[r][n];
                std::vector<std::string> terms;
                for (size_t k = c + 1; k < n; ++k) {
                    double coeff = mat[r][k];
                    if (std::abs(coeff) < kEpsilon)
                        continue;
                    // Use the parameter name (e.g. "t") if this column is a
                    // free variable, otherwise use the variable name directly.
                    const std::string& sym = (exprs.count(var_order[k]))
                                                 ? exprs.at(var_order[k])
                                                 : var_order[k];
                    std::ostringstream oss;
                    // Format coefficient·symbol term.
                    if (std::abs(coeff + 1.0) < kCoeffTol)
                        oss << "-" << sym;
                    else if (std::abs(coeff - 1.0) < kCoeffTol)
                        oss << sym;
                    else {
                        std::string cs = std::to_string(-coeff);
                        size_t      dp = cs.find('.');
                        if (dp != std::string::npos) {
                            cs.erase(cs.find_last_not_of('0') + 1);
                            if (cs.back() == '.')
                                cs.pop_back();
                        }
                        oss << cs << sym;
                    }
                    terms.push_back(oss.str());
                }

                std::ostringstream expr;
                // Constant part.
                double             val = rhs / mat[r][c];
                if (terms.empty()) {
                    std::string vs = std::to_string(val);
                    size_t      dp = vs.find('.');
                    if (dp != std::string::npos) {
                        vs.erase(vs.find_last_not_of('0') + 1);
                        if (vs.back() == '.')
                            vs.pop_back();
                    }
                    expr << vs;
                } else {
                    if (std::abs(val) > kEpsilon) {
                        std::string vs = std::to_string(val);
                        size_t      dp = vs.find('.');
                        if (dp != std::string::npos) {
                            vs.erase(vs.find_last_not_of('0') + 1);
                            if (vs.back() == '.')
                                vs.pop_back();
                        }
                        expr << vs;
                        for (auto& t : terms) {
                            if (!t.empty() && t[0] == '-')
                                expr << " - " << t.substr(1);
                            else
                                expr << " - " << t;
                        }
                    } else {
                        bool first = true;
                        for (auto& t : terms) {
                            if (!first) {
                                if (!t.empty() && t[0] == '-')
                                    expr << " + " << t.substr(1);
                                else
                                    expr << " + " << t;
                            } else {
                                if (!t.empty() && t[0] == '-') {
                                    expr << "-" << t.substr(1);
                                } else {
                                    expr << t;
                                }
                                first = false;
                            }
                        }
                    }
                }
                exprs[var_order[c]] = expr.str();
            }

            return exprs;
        }

        // ── LU decomposition (Doolittle with partial pivoting)
        // ───────────────────────

        /// Factor P·A = L·U using partial pivoting.
        /// On return: L and U are stored combined in `lu` (L below diagonal, U
        /// on/above). `perm[i]` records the row permuted to position i. Returns
        /// the smallest absolute diagonal element of U seen.
        double lu_factor(Matrix& lu, std::vector<size_t>& perm, size_t n) {
            perm.resize(n);
            for (size_t i = 0; i < n; ++i)
                perm[i] = i;

            double min_diag = 1e18;
            for (size_t k = 0; k < n; ++k) {
                // Partial pivot.
                size_t max_row = k;
                double max_val = std::abs(lu[k][k]);
                for (size_t r = k + 1; r < n; ++r) {
                    if (std::abs(lu[r][k]) > max_val) {
                        max_val = std::abs(lu[r][k]);
                        max_row = r;
                    }
                }
                if (max_row != k) {
                    std::swap(lu[k], lu[max_row]);
                    std::swap(perm[k], perm[max_row]);
                }

                if (std::abs(lu[k][k]) < kEpsilon)
                    continue; // singular column

                if (std::abs(lu[k][k]) < min_diag)
                    min_diag = std::abs(lu[k][k]);

                for (size_t r = k + 1; r < n; ++r) {
                    lu[r][k] /= lu[k][k];
                    for (size_t c = k + 1; c < n; ++c)
                        lu[r][c] -= lu[r][k] * lu[k][c];
                }
            }
            return min_diag;
        }

        /// Forward substitution: solve L·y = P·b, where L is unit
        /// lower-triangular stored in `lu` (below the diagonal).
        std::vector<double> forward_solve(const Matrix&              lu,
                                          const std::vector<size_t>& perm,
                                          const std::vector<double>& b,
                                          size_t                     n) {
            std::vector<double> y(n, 0.0);
            for (size_t i = 0; i < n; ++i) {
                double s = b[perm[i]];
                for (size_t j = 0; j < i; ++j)
                    s -= lu[i][j] * y[j];
                y[i] = s;
            }
            return y;
        }

        /// Back substitution: solve U·x = y, where U is upper-triangular stored
        /// in `lu`.
        std::vector<double> back_solve(const Matrix&              lu,
                                       const std::vector<double>& y, size_t n) {
            std::vector<double> x(n, 0.0);
            for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
                double s = y[static_cast<size_t>(i)];
                for (size_t j = static_cast<size_t>(i) + 1; j < n; ++j)
                    s -= lu[static_cast<size_t>(i)][j] * x[j];
                x[static_cast<size_t>(i)] =
                    s / lu[static_cast<size_t>(i)][static_cast<size_t>(i)];
            }
            return x;
        }

    } // anonymous namespace

    MatrixSolver::MatrixSolver(const std::string& input) : input_(input) {}

    Result<SystemSolveResult>
    MatrixSolver::solve(const std::vector<LinearForm>&  forms,
                        const std::vector<std::string>& var_order,
                        const SolveSystemOptions&       opts) {
        if (forms.empty() || var_order.empty()) {
            return Result<SystemSolveResult>::err(
                errors::system_no_solution("empty system", Span{}, input_));
        }

        const size_t      n   = var_order.size();

        // Build augmented matrix.
        Matrix            aug = build_augmented(forms, var_order);

        SystemSolveResult res;

        if (opts.method == SolveMethod::LU && forms.size() == n) {
            // ── LU path (square systems only)
            // ───────────────────────────────── Extract A (square) and b.
            Matrix              A(n, std::vector<double>(n));
            std::vector<double> b(n);
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = 0; j < n; ++j)
                    A[i][j] = aug[i][j];
                b[i] = aug[i][n];
            }

            Matrix              lu = A;
            std::vector<size_t> perm;
            double              min_piv = lu_factor(lu, perm, n);

            res.smallest_pivot          = min_piv;

            // Check rank by counting non-zero diagonal entries.
            int rank                    = 0;
            for (size_t i = 0; i < n; ++i)
                if (std::abs(lu[i][i]) > kPivotTol)
                    ++rank;

            res.rank_A = rank;
            res.rank_Ab =
                rank; // For square LU, check augmented separately below.

            // Check augmented rank by running a Gauss step on the full matrix.
            {
                Matrix aug_check = aug;
                gauss_eliminate(aug_check, n);
                res.rank_Ab = count_rank(aug_check, n + 1);
                res.rank_A  = count_rank(aug_check, n);
            }

            if (res.rank_Ab > res.rank_A) {
                return Result<SystemSolveResult>::err(
                    errors::system_no_solution(
                        "system has no solution (rank([A|b]) > rank(A))",
                        Span{}, input_));
            }
            if (res.rank_A < static_cast<int>(n)) {
                if (!opts.free_vars) {
                    return Result<SystemSolveResult>::err(
                        errors::system_infinite_solutions(
                            "system has infinitely many solutions", Span{},
                            input_));
                }
                // Fall through to Gauss for parameterisation.
                goto gauss_path;
            }

            {
                auto y        = forward_solve(lu, perm, b, n);
                auto x        = back_solve(lu, y, n);
                res.is_unique = true;
                for (size_t i = 0; i < n; ++i)
                    res.solutions[var_order[i]] = x[i];
                return Result<SystemSolveResult>::ok(std::move(res));
            }
        }

    gauss_path: {
        Matrix work        = aug;
        double min_piv     = gauss_eliminate(work, n);
        res.smallest_pivot = min_piv;

        res.rank_A         = count_rank(work, n);
        res.rank_Ab        = count_rank(work, n + 1);

        if (res.rank_Ab > res.rank_A) {
            return Result<SystemSolveResult>::err(errors::system_no_solution(
                "system has no solution (rank([A|b]) > rank(A))", Span{},
                input_));
        }

        if (res.rank_A < static_cast<int>(n)) {
            if (!opts.free_vars) {
                return Result<SystemSolveResult>::err(
                    errors::system_infinite_solutions(
                        "system has infinitely many solutions", Span{},
                        input_));
            }
            // Parameterise free variables.
            res.is_unique   = false;
            res.free_params = parameterise_free(work, var_order, res.free_vars);
            return Result<SystemSolveResult>::ok(std::move(res));
        }

        // Unique solution: back-substitution.
        std::vector<double> x;
        back_substitute(work, n, x);
        res.is_unique = true;
        for (size_t i = 0; i < n; ++i)
            res.solutions[var_order[i]] = x[i];
        return Result<SystemSolveResult>::ok(std::move(res));
    }
    }

} // namespace math_solver
