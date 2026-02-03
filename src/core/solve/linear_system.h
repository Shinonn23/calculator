#ifndef LINEAR_SYSTEM_H
#define LINEAR_SYSTEM_H

#include "../common/error.h"
#include "../common/fraction.h"
#include "linear_collector.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace math_solver {

    // Result type for system solving
    enum class SolutionType {
        Unique,     // Exactly one solution
        NoSolution, // Inconsistent system
        Infinite    // Infinitely many solutions (free variables)
    };

    // Result of solving a linear system
    struct SystemSolution {
        SolutionType             type = SolutionType::Unique;
        std::vector<std::string> variables;       // Variable names in order
        std::vector<double>      values;          // Solution values (if unique)
        std::vector<std::string> free_variables;  // Free variables (if infinite)
        std::vector<std::string> warnings;

        std::string to_string(bool as_fraction = false) const {
            std::ostringstream oss;

            switch (type) {
            case SolutionType::NoSolution:
                oss << "No solution (inconsistent system)";
                break;

            case SolutionType::Infinite:
                oss << "Infinite solutions";
                if (!free_variables.empty()) {
                    oss << "\nFree variables: ";
                    for (size_t i = 0; i < free_variables.size(); ++i) {
                        if (i > 0)
                            oss << ", ";
                        oss << free_variables[i];
                    }
                }
                break;

            case SolutionType::Unique:
                for (size_t i = 0; i < variables.size(); ++i) {
                    if (i > 0)
                        oss << "\n";
                    oss << variables[i] << " = ";
                    if (as_fraction) {
                        Fraction frac = double_to_fraction(values[i]);
                        oss << frac.to_string();
                    } else {
                        double val = values[i];
                        if (std::abs(val) < 1e-12)
                            val = 0;
                        std::string val_str = std::to_string(val);
                        size_t      dot     = val_str.find('.');
                        if (dot != std::string::npos) {
                            val_str.erase(val_str.find_last_not_of('0') + 1);
                            if (val_str.back() == '.')
                                val_str.pop_back();
                        }
                        oss << val_str;
                    }
                }
                break;
            }

            return oss.str();
        }
    };

    // Augmented matrix for Gaussian elimination [A | b]
    class AugmentedMatrix {
        private:
        std::vector<std::vector<double>> data_;
        size_t                           rows_;
        size_t                           cols_;
        static constexpr double          EPSILON = 1e-12;

        public:
        AugmentedMatrix(size_t rows, size_t cols) : rows_(rows), cols_(cols) {
            data_.resize(rows, std::vector<double>(cols + 1, 0.0));
        }

        size_t rows() const { return rows_; }
        size_t cols() const { return cols_; }

        double& at(size_t r, size_t c) { return data_[r][c]; }
        double  at(size_t r, size_t c) const { return data_[r][c]; }
        double& rhs(size_t r) { return data_[r][cols_]; }
        double  rhs(size_t r) const { return data_[r][cols_]; }

        void swap_rows(size_t r1, size_t r2) {
            if (r1 != r2)
                std::swap(data_[r1], data_[r2]);
        }

        void scale_row(size_t r, double factor) {
            for (size_t c = 0; c <= cols_; ++c)
                data_[r][c] *= factor;
        }

        void add_scaled_row(size_t dst, size_t src, double factor) {
            for (size_t c = 0; c <= cols_; ++c)
                data_[dst][c] += factor * data_[src][c];
        }

        // Find best pivot row for column c, starting from row r
        size_t find_pivot(size_t r, size_t c) const {
            size_t best     = rows_;
            double best_abs = EPSILON;
            for (size_t i = r; i < rows_; ++i) {
                double abs_val = std::abs(data_[i][c]);
                if (abs_val > best_abs) {
                    best_abs = abs_val;
                    best     = i;
                }
            }
            return best;
        }

        // Perform Gaussian elimination to Reduced Row Echelon Form
        std::vector<size_t> to_rref() {
            std::vector<size_t> pivot_cols;
            size_t              current_row = 0;

            for (size_t col = 0; col < cols_ && current_row < rows_; ++col) {
                size_t pivot_row = find_pivot(current_row, col);
                if (pivot_row == rows_)
                    continue;

                swap_rows(current_row, pivot_row);
                double pivot = data_[current_row][col];
                scale_row(current_row, 1.0 / pivot);

                // Eliminate all other rows (both above and below)
                for (size_t r = 0; r < rows_; ++r) {
                    if (r != current_row && std::abs(data_[r][col]) > EPSILON) {
                        add_scaled_row(r, current_row, -data_[r][col]);
                    }
                }

                pivot_cols.push_back(col);
                ++current_row;
            }

            // Clean up near-zero values
            for (size_t r = 0; r < rows_; ++r) {
                for (size_t c = 0; c <= cols_; ++c) {
                    if (std::abs(data_[r][c]) < EPSILON)
                        data_[r][c] = 0.0;
                }
            }

            return pivot_cols;
        }

        // Check for inconsistency: row of form [0 0 ... 0 | b] where b != 0
        bool is_inconsistent() const {
            for (size_t r = 0; r < rows_; ++r) {
                bool all_zero = true;
                for (size_t c = 0; c < cols_; ++c) {
                    if (std::abs(data_[r][c]) > EPSILON) {
                        all_zero = false;
                        break;
                    }
                }
                if (all_zero && std::abs(data_[r][cols_]) > EPSILON)
                    return true;
            }
            return false;
        }

        // Pretty print the matrix
        std::string to_string() const {
            std::ostringstream oss;
            for (size_t r = 0; r < rows_; ++r) {
                oss << "[ ";
                for (size_t c = 0; c < cols_; ++c) {
                    oss << std::setw(8) << std::fixed << std::setprecision(3)
                        << data_[r][c] << " ";
                }
                oss << "| " << std::setw(8) << data_[r][cols_] << " ]\n";
            }
            return oss.str();
        }
    };

    // Linear system: collection of linear equations
    class LinearSystem {
        private:
        std::vector<LinearForm>  equations_;
        std::vector<std::string> variables_;

        public:
        LinearSystem() = default;

        // Add an equation to the system
        void add_equation(const LinearForm& eq) {
            equations_.push_back(eq);
            // Update variable list
            for (const auto& var : eq.variables()) {
                if (std::find(variables_.begin(), variables_.end(), var) ==
                    variables_.end()) {
                    variables_.push_back(var);
                }
            }
        }

        void set_variables(const std::vector<std::string>& vars) {
            variables_ = vars;
        }

        const std::vector<std::string>& variables() const { return variables_; }

        void sort_variables() {
            std::sort(variables_.begin(), variables_.end());
        }

        size_t num_equations() const { return equations_.size(); }
        size_t num_variables() const { return variables_.size(); }

        const std::vector<LinearForm>& equations() const { return equations_; }

        void clear() {
            equations_.clear();
            variables_.clear();
        }

        bool empty() const { return equations_.empty(); }

        // Build augmented matrix from the system
        AugmentedMatrix build_matrix() const {
            AugmentedMatrix matrix(equations_.size(), variables_.size());

            for (size_t r = 0; r < equations_.size(); ++r) {
                const auto& eq = equations_[r];
                for (size_t c = 0; c < variables_.size(); ++c) {
                    matrix.at(r, c) = eq.get_coeff(variables_[c]);
                }
                // RHS: LinearForm stores as coeffs - constant = 0, so RHS =
                // -constant
                matrix.rhs(r) = -eq.constant;
            }

            return matrix;
        }

        // Solve the system using Gaussian elimination
        SystemSolution solve() const {
            SystemSolution result;
            result.variables = variables_;

            if (equations_.empty()) {
                result.type           = SolutionType::Infinite;
                result.free_variables = variables_;
                return result;
            }

            if (variables_.empty()) {
                // All constants - check consistency
                for (const auto& eq : equations_) {
                    if (std::abs(eq.constant) > 1e-12) {
                        result.type = SolutionType::NoSolution;
                        return result;
                    }
                }
                result.type = SolutionType::Unique;
                return result;
            }

            // Build and solve matrix
            AugmentedMatrix     matrix     = build_matrix();
            std::vector<size_t> pivot_cols = matrix.to_rref();

            // Check for inconsistency
            if (matrix.is_inconsistent()) {
                result.type = SolutionType::NoSolution;
                return result;
            }

            // Check if we have unique solution (pivot in every column)
            if (pivot_cols.size() < variables_.size()) {
                result.type = SolutionType::Infinite;

                // Identify free variables
                std::set<size_t> pivot_set(pivot_cols.begin(), pivot_cols.end());
                for (size_t c = 0; c < variables_.size(); ++c) {
                    if (pivot_set.find(c) == pivot_set.end()) {
                        result.free_variables.push_back(variables_[c]);
                    }
                }

                return result;
            }

            // Unique solution - read from RREF matrix
            result.type = SolutionType::Unique;
            result.values.resize(variables_.size());

            for (size_t i = 0; i < pivot_cols.size(); ++i) {
                size_t col            = pivot_cols[i];
                result.values[col] = matrix.rhs(i);
            }

            return result;
        }
    };

} // namespace math_solver

#endif
