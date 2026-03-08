#include "math_command_parser.hpp"
#include "ast/command/math_command.hpp"
#include "diagnostics/kinds/command_errors.hpp"

#include <cctype>

namespace math_solver {

    // Returns the trailing flag token (e.g. "--no-save") if the payload ends
    // with one, otherwise returns an empty string.
    // A flag is defined as a whitespace-separated suffix starting with '-'
    // followed by an alpha character or another '-'.
    static std::string extract_trailing_flag(const std::string& s) {
        size_t e = s.find_last_not_of(" \t\r\n");
        if (e == std::string::npos)
            return "";
        size_t b = s.find_last_of(" \t\r\n", e);
        b        = (b == std::string::npos) ? 0 : b + 1;
        if (e - b + 1 >= 2 && s[b] == '-' &&
            (std::isalpha(static_cast<unsigned char>(s[b + 1])) ||
             s[b + 1] == '-'))
            return s.substr(b, e - b + 1);
        return "";
    }

    // Maps command string to MathCommand::Type.
    // - Assumes input is always one of the supported commands.
    // - Defaulting to Factor is intentional: parser should not reach here with
    // unknown commands.
    static MathCommand::Type resolve_type(const std::string& cmd) {
        if (cmd == ":solve")
            return MathCommand::Type::Solve;
        if (cmd == ":simplify")
            return MathCommand::Type::Simplify;
        if (cmd == ":expand")
            return MathCommand::Type::Expand;
        return MathCommand::Type::Factor;
    }

    Result<CommandPtr> MathCommandParser::parse(ITokenStream& stream) {
        // Entry point for parsing math commands.
        // Grammar:
        //   :cmd [flags] <expr>            -- flags must come before unquoted expr
        //   :cmd [flags] "<expr>" [flags]  -- quoted expr; flags allowed either side
        //
        // - Expects the stream to be positioned at the command token (e.g.,
        // ":solve").
        // - Advances past the command token before processing flags.
        // - The "-vars"/"--vars" flag consumes all subsequent Word/QuotedString
        // tokens as variable names.
        //   This is greedy and assumes no ambiguity with other flags.
        // - Any remaining tokens are treated as the command's argument
        // expression.
        MathCommand::Type type = resolve_type(stream.peek().value);
        stream.advance();
        bool                     isolated        = false;
        bool                     fraction        = false;
        bool                     show_matrix     = false;
        bool                     no_save         = false;
        bool                     show_rank       = false;
        bool                     detect_singular = false;
        bool                     free_vars_flag  = false;
        SolveMethod              method                = SolveMethod::Gauss;
        bool                     method_explicitly_set = false;
        std::vector<std::string> vars;

        // Shared flag handler — called in both phase 1 and phase 3.
        auto handle_flag = [&](const std::string& flag) {
            if (flag == "-isolated" || flag == "--isolated")
                isolated = true;
            else if (flag == "-fraction" || flag == "--fraction")
                fraction = true;
            else if (flag == "-exact" || flag == "--exact")
                fraction = true; // alias for --fraction
            else if (flag.rfind("--method=", 0) == 0 ||
                     flag.rfind("-method=", 0) == 0) {
                method_explicitly_set = true;
                std::string val = flag.substr(flag.find('=') + 1);
                if (val == "lu")
                    method = SolveMethod::LU;
                // else default Gauss
            } else if (flag == "-show-matrix" || flag == "--show-matrix")
                show_matrix = true;
            else if (flag == "-no-save" || flag == "--no-save")
                no_save = true;
            else if (flag == "-rank" || flag == "--rank")
                show_rank = true;
            else if (flag == "-detect-singular" || flag == "--detect-singular")
                detect_singular = true;
            else if (flag == "-free-vars" || flag == "--free-vars")
                free_vars_flag = true;
            else if (flag == "-vars" || flag == "--vars") {
                // Greedily consume all variable names after -vars/--vars.
                // Invariant: no other flags or arguments should appear between
                // -vars and its values.
                while (stream.peek_is(CommandTokenType::Word) ||
                       stream.peek_is(CommandTokenType::QuotedString)) {
                    vars.push_back(stream.advance().value);
                }
            }
        };

        // Phase 1: flags before expression.
        while (stream.peek_is(CommandTokenType::Flag)) {
            handle_flag(stream.advance().value);
        }

        // Phase 2: expression — quoted or unquoted.
        // - Quoted: lexer already strips the surrounding quotes from the value.
        //   After consuming the quoted string, a phase-3 flag pass follows.
        // - Unquoted: consume the raw remainder as-is (existing behaviour).
        std::string payload;
        if (stream.peek_is(CommandTokenType::QuotedString)) {
            payload = stream.advance().value;
            // Phase 3: flags that appear after a quoted expression.
            while (stream.peek_is(CommandTokenType::Flag)) {
                handle_flag(stream.advance().value);
            }
        } else {
            payload = stream.consume_remaining();
            // Detect flags accidentally written after an unquoted expression.
            // Because consume_remaining() grabs everything verbatim, a trailing
            // flag like --no-save ends up baked into the payload and is silently
            // ignored by the math parser. Emit an error instead.
            if (auto flag = extract_trailing_flag(payload); !flag.empty()) {
                return Result<CommandPtr>::err(
                    errors::trailing_flag_after_expr(flag,
                                                     stream.raw_input()));
            }
        }

        // Construct MathCommand with parsed type and arguments.
        // - set_flags encodes all flag state; no further mutation after
        // construction.
        // - raw_input is preserved for error reporting or diagnostics.
        auto cmd =
            std::make_unique<MathCommand>(type, payload, stream.raw_input());
        cmd->set_flags(isolated, fraction, vars);
        cmd->set_system_flags(method, method_explicitly_set, show_matrix,
                              no_save, show_rank, detect_singular,
                              free_vars_flag);
        return Result<CommandPtr>::ok(std::move(cmd));
    }

} // namespace math_solver
