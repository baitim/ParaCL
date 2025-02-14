#pragma once

#include "ANSI_colors.hpp"
#include <iostream>

#ifndef yyFlexLexer
#include <../FlexLexer.h>
#endif

#include <fstream>
#include <sstream>

namespace yy {
    class lexer_t final : public yyFlexLexer {
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

        location get_location() const noexcept { return loc_; }

        int yylex() override;
    };
}