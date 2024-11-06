#pragma once

#include "ANSI_colors.hpp"
#include <iostream>

#ifndef yyFlexLexer
#include <FlexLexer.h>
#endif

#include <fstream>
#include <sstream>

namespace yy {

    struct error_t final {
        std::string msg_;
        error_t(const char*        msg) : msg_(msg) {}
        error_t(const std::string& msg) : msg_(msg) {}
    };

    class Lexer_t : public yyFlexLexer {
        location loc_;

    public:
        void update_new_token() {
            loc_.columns(yyleng);
            loc_.step();
        }

        void update_new_line() {
            loc_.lines(yyleng);
            loc_.step();
        }

        location get_location() const { return loc_; }

        int yylex() override;
    };
}