#include "result.hpp"
#include "ui/color.hpp"

namespace math_solver {

    std::string Error::format() const {
        std::string out;

        // Header
        const bool  is_warn = (level == "warning");
        out += is_warn ? ansi::yellow : ansi::red;
        out += ansi::bold;
        out += level;
        if (!code.empty())
            out += "[" + code + "]";
        out += std::string(ansi::reset) + ansi::bold;
        out += ": " + message;
        out += std::string(ansi::reset) + "\n";

        if (input.empty()) {
            if (!help.empty())
                out += "  = help: " + help + "\n";
            if (!note.empty())
                out += "  = note: " + note + "\n";
            return out;
        }

        // Compute line/col within input
        size_t line_start = 0;
        for (size_t i = 0; i < span.start && i < input.size(); ++i)
            if (input[i] == '\n')
                line_start = i + 1;
        size_t col = span.start >= line_start ? span.start - line_start + 1 : 1;

        // Source line
        size_t line_end = input.find('\n', line_start);
        if (line_end == std::string::npos)
            line_end = input.size();
        std::string src      = input.substr(line_start, line_end - line_start);

        std::string line_str = std::to_string(loc.line);
        std::string margin_pad(line_str.size(), ' ');

        // Arrow
        out +=
            std::string(ansi::cyan) + " " + margin_pad + "--> " + ansi::reset;
        out += loc.file + ":" + line_str + ":" + std::to_string(col) + "\n";

        // Gutter + source
        out += std::string(ansi::cyan) + " " + margin_pad + " |" + ansi::reset +
               "\n";
        out += std::string(ansi::cyan) + " " + line_str + " | " + ansi::reset +
               src + "\n";

        // Carets
        out += std::string(ansi::cyan) + " " + margin_pad + " | " + ansi::reset;
        out += std::string(col - 1, ' ');
        size_t len = span.empty() ? 1 : span.length();
        out += is_warn ? ansi::yellow : ansi::red;
        out += std::string(ansi::bold) + std::string(len, '^');
        if (!inline_label.empty())
            out += " " + inline_label;
        out += std::string(ansi::reset) + "\n";

        // Help / note
        if (!help.empty())
            out += std::string(ansi::cyan) + " " + margin_pad + " =" +
                   ansi::reset + " " + ansi::bold + "help:" + ansi::reset +
                   " " + help + "\n";
        if (!note.empty())
            out += std::string(ansi::cyan) + " " + margin_pad + " =" +
                   ansi::reset + " " + ansi::bold + "note:" + ansi::reset +
                   " " + note + "\n";

        return out;
    }

} // namespace math_solver
