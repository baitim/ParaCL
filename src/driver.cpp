#include "ANSI_colors.hpp"
#include "ast.hpp"
#include "driver.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << print_red("Invalid argument: argc = 2, argv[1] = name of file\n");
        return 1;
    }

    yy::Lexer_t* lexer = new yy::Lexer_t;
    yy::Driver_t driver{lexer};
    ast::ast_t ast;

    try {
        driver.parse(argv[1], ast.root_);
    } catch (const char* error_message) {
        goto error;
    }

    try {
        ast.execute();
    } catch (const char* error_message) {
        std::cout << print_red(error_message) << "\n";
        goto error;
    }

    delete lexer;

    goto no_error;
error:
    delete lexer;
    return 1;

no_error:
    return 0;
}