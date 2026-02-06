#ifndef VALUE_H
#define VALUE_H

#include "format_utils.h"
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace math_solver {

    // ================================================================
    // Value — unified result type for the evaluation engine.
    //
    // SCOPE: Engine supports 1D vectors only.
    //        No matrix, interval, or complex number support.
    //        Future types are listed below as comments for reference.
    // ================================================================
    class Value {
        public:
        // Current variant members:
        //   double              — scalar numeric value
        //   std::vector<double> — 1D numeric vector (e.g., multiple roots)
        //
        // Future (not yet implemented):
        //   Interval            — interval arithmetic
        //   Complex             — complex number
        //   Matrix              — 2D numeric matrix
        using Storage = std::variant<double, std::vector<double>>;

        private:
        Storage storage_;

        public:
        // ── Constructors ────────────────────────────────────────
        Value() : storage_(0.0) {}
        explicit Value(double v) : storage_(v) {}
        explicit Value(std::vector<double> v) : storage_(std::move(v)) {}

        // ── Named constructors ──────────────────────────────────
        static Value scalar(double v) { return Value(v); }

        static Value vector(std::vector<double> v) {
            return Value(std::move(v));
        }

        // ── Type queries ────────────────────────────────────────
        bool is_scalar() const {
            return std::holds_alternative<double>(storage_);
        }

        bool is_vector() const {
            return std::holds_alternative<std::vector<double>>(storage_);
        }

        // ── Accessors ───────────────────────────────────────────
        double as_scalar() const {
            if (is_scalar()) {
                return std::get<double>(storage_);
            }
            // Single-element vector → scalar
            const auto& vec = std::get<std::vector<double>>(storage_);
            if (vec.size() == 1) {
                return vec[0];
            }
            throw std::runtime_error("cannot convert vector with " +
                                     std::to_string(vec.size()) +
                                     " elements to scalar");
        }

        const std::vector<double>& as_vector() const {
            if (is_vector()) {
                return std::get<std::vector<double>>(storage_);
            }
            throw std::runtime_error("value is a scalar, not a vector");
        }

        // Convert to vector regardless of type (scalar → single-element vec)
        std::vector<double> to_vector() const {
            if (is_vector()) {
                return std::get<std::vector<double>>(storage_);
            }
            return {std::get<double>(storage_)};
        }

        size_t size() const {
            if (is_scalar())
                return 1;
            return std::get<std::vector<double>>(storage_).size();
        }

        // ── String formatting ───────────────────────────────────
        std::string to_string() const {
            if (is_scalar()) {
                return format_double(std::get<double>(storage_));
            }
            const auto& vec    = std::get<std::vector<double>>(storage_);
            std::string result = "[";
            for (size_t i = 0; i < vec.size(); ++i) {
                if (i > 0)
                    result += ", ";
                result += format_double(vec[i]);
            }
            result += "]";
            return result;
        }

        // ── Raw access for internal use ─────────────────────────
        const Storage& storage() const { return storage_; }
    };

    // ================================================================
    // EvaluationConfig — per-call evaluation settings.
    //
    // Passed to Evaluator per invocation, NOT stored in Context.
    // This avoids global state, ensuring thread safety and nested
    // evaluation correctness.
    // ================================================================
    enum class EvalMode {
        Numeric,  // Evaluate to numeric values (default)
        Symbolic, // Expand + constant-fold only (stub — future)
        Vector    // Full vector broadcasting support
    };

    struct EvaluationConfig {
        EvalMode mode = EvalMode::Numeric;
    };

} // namespace math_solver

#endif
