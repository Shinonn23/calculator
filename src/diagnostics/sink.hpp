#pragma once

#include "diagnostics/result.hpp"
#include "ui/color.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace math_solver {

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

        void push(Diagnostic d) {
            if (d.level == "warning") {
                if (opts_.dedup && is_duplicate(d, warnings_))
                    return;
                warnings_.push_back(std::move(d));
            } else {
                if (error_count_ >= opts_.max_errors) {
                    suppressed_++;
                    return;
                }
                if (opts_.dedup && is_duplicate(d, errors_))
                    return;
                errors_.push_back(std::move(d));
                error_count_++;
            }
        }

        template <typename T> void push(const Result<T>& result) {
            if (result.failed())
                push(result.error());
        }

        template <typename T> void push(const MultiResult<T>& result) {
            for (const auto& e : result.errors)
                push(e);
            for (const auto& w : result.warnings)
                push(w);
        }

        void push_output(std::string text) {
            outputs_.push_back(std::move(text));
        }

        bool   has_outputs() const { return !outputs_.empty(); }

        size_t flush_outputs(std::ostream& out = std::cout) {
            if (outputs_.empty())
                return 0;
            size_t n = outputs_.size();
            for (const auto& s : outputs_) {
                out << s;
            }
            outputs_.clear();
            return n;
        }

        void   clear_outputs() { outputs_.clear(); }

        bool   has_errors() const { return !errors_.empty(); }
        bool   has_warnings() const { return !warnings_.empty(); }
        bool   empty() const { return errors_.empty() && warnings_.empty(); }
        size_t error_count() const { return error_count_; }
        size_t warning_count() const { return warnings_.size(); }

        const std::vector<Diagnostic>& errors() const { return errors_; }
        const std::vector<Diagnostic>& warnings() const { return warnings_; }

        size_t                         flush(std::ostream& out = std::cout) {
            flush_outputs(out);

            if (empty() && suppressed_ == 0)
                return 0;

            std::vector<Diagnostic> to_render;

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
                to_render = merge_by_location(errors_, warnings_);
            }

            for (const auto& d : to_render)
                out << d.format();

            if (suppressed_ > 0) {
                out << "\n  " << ansi::dim << "... " << suppressed_
                    << " further error(s) suppressed (limit: "
                    << opts_.max_errors << ")" << ansi::reset << "\n";
            }

            size_t n = error_count_;
            clear();
            return n;
        }

        void flush_summary(const std::string& source, size_t total_lines,
                           std::ostream& out = std::cout) {
            size_t errs  = error_count_;
            size_t warns = warnings_.size();
            flush(out);

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
            outputs_.clear();
            error_count_ = 0;
            suppressed_  = 0;
        }

        private:
        Options                  opts_;
        std::vector<Diagnostic>  errors_;
        std::vector<Diagnostic>  warnings_;
        std::vector<std::string> outputs_;
        size_t                   error_count_ = 0;
        size_t                   suppressed_  = 0;

        static bool              is_duplicate(const Diagnostic&              d,
                                              const std::vector<Diagnostic>& existing) {
            return std::any_of(
                existing.begin(), existing.end(), [&](const Diagnostic& x) {
                    return x.code == d.code && x.message == d.message &&
                           x.loc.line == d.loc.line;
                });
        }

        static void sort_by_location(std::vector<Diagnostic>& v) {
            std::stable_sort(v.begin(), v.end(),
                             [](const Diagnostic& a, const Diagnostic& b) {
                                 if (a.loc.file != b.loc.file)
                                     return a.loc.file < b.loc.file;
                                 return a.loc.line < b.loc.line;
                             });
        }

        static std::vector<Diagnostic>
        merge_by_location(const std::vector<Diagnostic>& a,
                          const std::vector<Diagnostic>& b) {
            std::vector<Diagnostic> out;
            out.reserve(a.size() + b.size());
            out.insert(out.end(), a.begin(), a.end());
            out.insert(out.end(), b.begin(), b.end());
            sort_by_location(out);
            return out;
        }
    };

} // namespace math_solver
