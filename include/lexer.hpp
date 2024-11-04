#pragma once

#ifndef yyFlexLexer
#include <FlexLexer.h>
#endif

namespace yy {
class Lexer_t : public yyFlexLexer {
    location loc;

public:
    void update_new_token() {
        loc.columns(yyleng);
        loc.step();
    }

    void update_new_line() {
        loc.lines(yyleng);
        loc.step();
    }

    location get_location() const { return loc; }

    int yylex() override;
};
}