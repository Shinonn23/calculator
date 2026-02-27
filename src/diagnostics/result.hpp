#pragma once

#include "diagnostics/diagnostic.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace math_solver {

    template <typename T> class Result {
        std::variant<T, Diagnostic> data_;

        public:
        Result(T value) : data_(std::move(value)) {}
        Result(Diagnostic diag) : data_(std::move(diag)) {}

        static Result ok(T value) { return Result(std::move(value)); }
        static Result err(Diagnostic d) { return Result(std::move(d)); }

        bool     ok() const { return std::holds_alternative<T>(data_); }
        bool     failed() const { return !ok(); }
        explicit operator bool() const { return ok(); }

        T& operator*() {
            assert(ok() && "dereferenced failed Result");
            return std::get<T>(data_);
        }
        const T& operator*() const {
            assert(ok() && "dereferenced failed Result");
            return std::get<T>(data_);
        }
        T*       operator->() { return &**this; }
        const T* operator->() const { return &**this; }

        const Diagnostic& error() const {
            assert(failed() && "called error() on successful Result");
            return std::get<Diagnostic>(data_);
        }
        Diagnostic& error() {
            assert(failed() && "called error() on successful Result");
            return std::get<Diagnostic>(data_);
        }

        template <typename F>
        auto map(F&& f) const -> Result<decltype(f(std::declval<T>()))> {
            using U = decltype(f(std::declval<T>()));
            if (ok())
                return Result<U>::ok(f(**this));
            return Result<U>::err(error());
        }

        template <typename F> Result<T> map_error(F&& f) const {
            if (failed())
                return Result<T>::err(f(error()));
            return *this;
        }

        Result<T> with_location(const SourceLocation& loc) && {
            if (failed())
                return Result<T>::err(error().with_location(loc));
            return std::move(*this);
        }

        Result<T> with_location(const std::string& file, size_t line) && {
            return std::move(*this).with_location(
                SourceLocation::from_file(file, line));
        }
    };

    template <typename T> struct MultiResult {
        std::optional<T>        value;
        std::vector<Diagnostic> errors;
        std::vector<Diagnostic> warnings;

        bool     ok() const { return value.has_value() && errors.empty(); }
        explicit operator bool() const { return ok(); }

        static MultiResult ok(T v) {
            MultiResult r;
            r.value = std::move(v);
            return r;
        }

        static MultiResult err(std::vector<Diagnostic> errs) {
            MultiResult r;
            r.errors = std::move(errs);
            return r;
        }

        MultiResult& add_warning(Diagnostic w) {
            warnings.push_back(std::move(w));
            return *this;
        }
    };

} // namespace math_solver
