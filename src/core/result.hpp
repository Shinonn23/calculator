#pragma once

#include "span.hpp"
#include <cassert>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace math_solver {

    // ── Error ─────────────────────────────────────────────────────────────
    //
    // Plain value type representing a diagnostic error or warning.
    // No inheritance, no virtual dispatch.
    //
    // Design:
    // - Parser/lexer fills: message, span, input, code, label
    // - Runner fills: loc (via with_location)
    // - Handler fills: help, note (via with_help, with_note)
    //
    // All methods return Error by value for chaining without mutation
    // of the original — this makes catch-site augmentation explicit.

    struct SourceLocation {
        std::string           file = "<repl>";
        size_t                line = 1;
        size_t                col  = 1;

        static SourceLocation repl() { return {}; }
        static SourceLocation from_file(const std::string& f, size_t l) {
            return {f, l, 1};
        }
        bool is_repl() const { return file == "<repl>"; }
    };

    struct Error {
        // ── Core fields (set at throw/construction site) ──────────────────
        std::string    message;
        std::string    level        = "error"; // "error" | "warning"
        std::string    code         = "";
        std::string    inline_label = "";
        Span           span;
        std::string    input; // raw source line

        // ── Augmentation fields (set at catch/handler site) ───────────────
        std::string    help = "";
        std::string    note = "";
        SourceLocation loc; // injected by runner

        // ── Factory helpers ───────────────────────────────────────────────

        static Error make(const std::string& msg, const std::string& code = "",
                          const Span& span = {}, const std::string& input = "",
                          const std::string& label = "") {
            Error e;
            e.message      = msg;
            e.code         = code;
            e.span         = span;
            e.input        = input;
            e.inline_label = label;
            return e;
        }

        static Error warning(const std::string& msg, const Span& span = {},
                             const std::string& input = "") {
            Error e;
            e.message = msg;
            e.level   = "warning";
            e.span    = span;
            e.input   = input;
            return e;
        }

        // ── Fluent augmentation (returns new Error, original unchanged) ────
        [[nodiscard]] Error with_help(const std::string& h) const {
            Error copy = *this;
            copy.help  = h;
            return copy;
        }
        [[nodiscard]] Error with_note(const std::string& n) const {
            Error copy = *this;
            copy.note  = n;
            return copy;
        }
        [[nodiscard]] Error with_label(const std::string& l) const {
            Error copy        = *this;
            copy.inline_label = l;
            return copy;
        }
        [[nodiscard]] Error with_location(const SourceLocation& l) const {
            Error copy = *this;
            copy.loc   = l;
            return copy;
        }
        [[nodiscard]] Error with_location(const std::string& file,
                                          size_t             line) const {
            return with_location(SourceLocation::from_file(file, line));
        }

        // ── Rendering ─────────────────────────────────────────────────────
        std::string format() const; // impl in error.cpp
    };

    // ── Result<T> ─────────────────────────────────────────────────────────
    //
    // Discriminated union of a success value T or an Error.
    // Models std::expected<T, Error> without requiring C++23.
    //
    // Usage:
    //   Result<ExprPtr> parse_expr();
    //
    //   auto r = parse_expr();
    //   if (!r) { emit(r.error()); return; }
    //   use(*r);

    template <typename T> class Result {
        std::variant<T, Error> data_;

        public:
        // Construction
        Result(T value) : data_(std::move(value)) {}
        Result(Error error) : data_(std::move(error)) {}

        // Named constructors for clarity at call sites
        static Result ok(T value) { return Result(std::move(value)); }
        static Result err(Error e) { return Result(std::move(e)); }

        // Query
        bool          ok() const { return std::holds_alternative<T>(data_); }
        bool          failed() const { return !ok(); }
        explicit      operator bool() const { return ok(); }

        // Access — assert on wrong state
        T&            operator*() {
            assert(ok() && "dereferenced failed Result");
            return std::get<T>(data_);
        }
        const T& operator*() const {
            assert(ok() && "dereferenced failed Result");
            return std::get<T>(data_);
        }
        T*           operator->() { return &**this; }
        const T*     operator->() const { return &**this; }

        const Error& error() const {
            assert(failed() && "called error() on successful Result");
            return std::get<Error>(data_);
        }
        Error& error() {
            assert(failed() && "called error() on successful Result");
            return std::get<Error>(data_);
        }

        // Functional combinators
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

        // Inject location into error without unpacking
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

    // ── MultiResult<T> ────────────────────────────────────────────────────
    //
    // For operations that can produce multiple warnings alongside a value,
    // or collect multiple errors (e.g. script loading).

    template <typename T> struct MultiResult {
        std::optional<T>   value;
        std::vector<Error> errors;
        std::vector<Error> warnings;

        bool     ok() const { return value.has_value() && errors.empty(); }
        explicit operator bool() const { return ok(); }

        static MultiResult ok(T v) {
            MultiResult r;
            r.value = std::move(v);
            return r;
        }
        static MultiResult err(std::vector<Error> errs) {
            MultiResult r;
            r.errors = std::move(errs);
            return r;
        }

        MultiResult& add_warning(Error w) {
            warnings.push_back(std::move(w));
            return *this;
        }
    };

} // namespace math_solver