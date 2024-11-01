#include "ast.hpp"
#include "driver.hpp"

int yyFlexLexer::yywrap() {
    return 1;
}

int main() {
    FlexLexer* lexer = new yyFlexLexer;
    yy::Driver_t driver(lexer);
    ast::ast_t ast;
    driver.parse(ast.root_);
    ast.calculate();
    ast.print();
    delete lexer;
}