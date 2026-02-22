#pragma once

#include "result.hpp"
#include "ui/color.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace math_solver {

    // ── DiagnosticSink ────────────────────────────────────────────────────
    //
    // Collects errors and warnings from all layers during command execution.
    // Passed by reference through the call chain; never stored globally.
    //
    // Design decisions:
    // - Parameter-passing: explicit coupling, no hidden state, testable.
    // - Batch flush: accumulate everything, render once at end of command.
    // - Dedup: identical (code, message, loc) pairs are merged.
    // - Grouping: errors rendered before warnings, sorted by location.
    // - Limit: after max_errors, further errors are suppressed with a note.
    //
    // Invariant: push() is cheap — no I/O, no formatting.
    //            flush() is the only site that writes to output.
    //            flush() clears all accumulated diagnostics.

    class DiagnosticSink {
        public:
        struct Options {
            size_t max_errors;
            bool   dedup;
            bool   sort_by_loc;
            bool   warnings_last;

            Options(size_t max_errors_ = 20, bool dedup_ = true,
                    bool sort_by_loc_ = true, bool warnings_last_ = true)
                : max_errors(max_errors_), dedup(dedup_),
                  sort_by_loc(sort_by_loc_), warnings_last(warnings_last_) {}
        };

        explicit DiagnosticSink(Options opts = {}) : opts_(opts) {}

        // ── Push ─────────────────────────────────────────────────────────

        void push(Error e) {
            if (e.level == "warning") {
                if (opts_.dedup && is_duplicate(e, warnings_))
                    return;
                warnings_.push_back(std::move(e));
            } else {
                if (error_count_ >= opts_.max_errors) {
                    suppressed_++;
                    return;
                }
                if (opts_.dedup && is_duplicate(e, errors_))
                    return;
                errors_.push_back(std::move(e));
                error_count_++;
            }
        }

        // Push all errors from a failed Result.
        // No-op if Result is ok.
        template <typename T> void push(const Result<T>& result) {
            if (result.failed())
                push(result.error());
        }

        // Push all errors/warnings from a MultiResult.
        template <typename T> void push(const MultiResult<T>& result) {
            for (const auto& e : result.errors)
                push(e);
            for (const auto& w : result.warnings)
                push(w);
        }

        // ── Query ─────────────────────────────────────────────────────────

        bool   has_errors() const { return !errors_.empty(); }
        bool   has_warnings() const { return !warnings_.empty(); }
        bool   empty() const { return errors_.empty() && warnings_.empty(); }
        size_t error_count() const { return error_count_; }
        size_t warning_count() const { return warnings_.size(); }

        const std::vector<Error>& errors() const { return errors_; }
        const std::vector<Error>& warnings() const { return warnings_; }

        // ── Flush ─────────────────────────────────────────────────────────
        //
        // Renders all diagnostics to `out`, sorted and grouped per options.
        // Clears all state after rendering.
        // Returns number of errors flushed (for exit code / HistoryStatus).

        size_t                    flush(std::ostream& out = std::cout) {
            if (empty() && suppressed_ == 0)
                return 0;

            std::vector<Error> to_render;

            if (opts_.sort_by_loc) {
                sort_by_location(errors_);
                sort_by_location(warnings_);
            }

            if (opts_.warnings_last) {
                to_render.insert(to_render.end(), errors_.begin(),
                                                    errors_.end());
                to_render.insert(to_render.end(), warnings_.begin(),
                                                    warnings_.end());
            } else {
                // interleave by location
                to_render = merge_by_location(errors_, warnings_);
            }

            for (const auto& e : to_render)
                out << e.format();

            if (suppressed_ > 0) {
                out << "\n  " << ansi::dim << "... " << suppressed_
                    << " further error(s) suppressed (limit: "
                    << opts_.max_errors << ")" << ansi::reset << "\n";
            }

            size_t n = error_count_;
            clear();
            return n;
        }

        // Flush summary only — used by run_script for the final line.
        void flush_summary(const std::string& source, size_t total_lines,
                           std::ostream& out = std::cout) {
            size_t errs  = error_count_;
            size_t warns = warnings_.size();
            flush(out); // render all first

            out << "  Loaded '" << source << "' (" << total_lines << " line(s)";
            if (errs)
                out << ", " << ansi::red << errs << " error(s)" << ansi::reset;
            if (warns)
                out << ", " << ansi::yellow << warns << " warning(s)"
                    << ansi::reset;
            out << ")\n";
        }

        void clear() {
            errors_.clear();
            warnings_.clear();
            error_count_ = 0;
            suppressed_  = 0;
        }

        private:
        Options            opts_;
        std::vector<Error> errors_;
        std::vector<Error> warnings_;
        size_t             error_count_ = 0;
        size_t             suppressed_  = 0;

        // Dedup: same code + message + loc.line = duplicate.
        static bool        is_duplicate(const Error&              e,
                                        const std::vector<Error>& existing) {
            return std::any_of(
                existing.begin(), existing.end(), [&](const Error& x) {
                    return x.code == e.code && x.message == e.message &&
                           x.loc.line == e.loc.line;
                });
        }

        static void sort_by_location(std::vector<Error>& v) {
            std::stable_sort(v.begin(), v.end(),
                             [](const Error& a, const Error& b) {
                                 if (a.loc.file != b.loc.file)
                                     return a.loc.file < b.loc.file;
                                 return a.loc.line < b.loc.line;
                             });
        }

        static std::vector<Error>
        merge_by_location(const std::vector<Error>& a,
                          const std::vector<Error>& b) {
            std::vector<Error> out;
            out.reserve(a.size() + b.size());
            out.insert(out.end(), a.begin(), a.end());
            out.insert(out.end(), b.begin(), b.end());
            sort_by_location(out);
            return out;
        }
    };

} // namespace math_solver