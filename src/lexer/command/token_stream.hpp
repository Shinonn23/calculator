#pragma once

#include "command_token.hpp"
#include <string>

namespace math_solver {

    class ITokenStream {
        public:
        virtual ~ITokenStream()                 = default;

        virtual CommandToken       peek() const = 0; // ดู token ปัจจุบัน
        virtual CommandToken       advance()    = 0; // กิน token แล้วคืน token นั้น
        virtual bool               is_eof() const    = 0;
        virtual const std::string& raw_input() const = 0;

        // Helper ที่ทุก subparser จะใช้บ่อย
        bool        peek_is(CommandTokenType t) const { return peek().is(t); }

        std::string consume_remaining() const {
            // คืน raw_input ตั้งแต่ offset ของ peek() ไปจนจบ
            const CommandToken& tok = peek();
            if (is_eof())
                return "";
            const std::string& input = raw_input();
            if (tok.start >= input.size())
                return "";
            return input.substr(tok.start);
        }
    };

} // namespace math_solver
