#pragma once

#include "core/span.hpp"
#include "diagnostics/diagnostic.hpp"
#include "runtime/context/context.hpp"
#include "ui/suggestions.hpp"

#include <string>

namespace math_solver {

    namespace errors {

        inline Diagnostic var_not_found(const std::string& raw, const Span& span,
                                        const std::string& name,
                                        const Context&     ctx,
                                        const std::string& file, size_t line) {
            auto d = Diagnostic::make("variable `" + name + "` not found",
                                      "E0425", span, raw, "unknown variable")
                         .with_location(file, line);
            if (auto m = suggest(name, ctx.all_names()))
                d.help = "a variable with a similar name exists: `" + *m + "`";
            return d;
        }

        inline Diagnostic missing_var_name(const std::string& raw,
                                           const Span&        span,
                                           const std::string& usage,
                                           const std::string& file,
                                           size_t             line) {
            auto d = Diagnostic::make("missing variable name", "E0401", span,
                                      raw, "variable name expected here")
                         .with_location(file, line);
            d.help = "Usage: " + usage;
            return d;
        }

        inline Diagnostic
        not_support_multiple(const std::string&              raw,
                             const Span&                     span,
                             const std::vector<std::string>& vars,
                             const std::string& file, size_t line) {
            std::string var_list;
            for (size_t i = 0; i < vars.size(); ++i) {
                if (i > 0)
                    var_list += ", ";
                var_list += "`" + vars[i] + "`";
            }
            auto d = Diagnostic::make(
                         "multiple variable names not supported: " + var_list,
                         "E0405", span, raw,
                         "only one variable name allowed in :set command")
                         .with_location(file, line);
            d.help = "Usage: `:set <var> <expr>` or `:unset <var>, <var>, ...`";
            return d;
        }

        inline Diagnostic reserved_keyword(const std::string& raw,
                                           const Span&        span,
                                           const std::string& name,
                                           const std::string& file,
                                           size_t             line) {
            auto d = Diagnostic::make("`" + name + "` is a reserved keyword",
                                      "E0402", span, raw, "reserved word")
                         .with_location(file, line);
            d.help = "choose a different variable name";
            return d;
        }

        inline Diagnostic invalid_identifier(const std::string& raw,
                                             const Span&        span,
                                             const std::string& name,
                                             const std::string& file,
                                             size_t             line) {
            auto d = Diagnostic::make("invalid variable name `" + name + "`",
                                      "E0403", span, raw, "invalid identifier")
                         .with_location(file, line);
            d.help = "names must start with a letter or `_` and contain only "
                     "alphanumeric characters";
            return d;
        }

        inline Diagnostic missing_expr(const std::string& raw, const Span& span,
                                       const std::string& /*var_name*/,
                                       const std::string& file, size_t line) {
            auto d = Diagnostic::make("missing expression", "E0404", span, raw,
                                      "expression expected after this")
                         .with_location(file, line);
            d.help = "Usage: `:set <var> <expr>`";
            return d;
        }

        inline Diagnostic self_reference(const std::string& raw,
                                         const Span&        span,
                                         const std::string& var_name,
                                         const std::string& file, size_t line) {
            auto d = Diagnostic::make(
                         "expression refers to `" + var_name +
                             "` which is the variable being assigned",
                         "E0406", span, raw, "self-reference not allowed")
                         .with_location(file, line);
            d.help = "use a different variable name or resolve `" + var_name +
                     "` before assigning";
            return d;
        }

    } // namespace errors

} // namespace math_solver
