#include "ANSI_colors.hpp"
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
    try {
        ast.execute();
    } catch (const char* error_message) {
        std::cout << print_red(error_message) << "\n";
        return 1;
    }
    delete lexer;
}