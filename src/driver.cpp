#include "driver.hpp"
#include <iostream>

int yyFlexLexer::yywrap() {
    return 1;
}

int main() {
    FlexLexer* lexer = new yyFlexLexer;
    yy::Driver_t driver(lexer);
    driver.parse();
}