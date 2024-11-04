#include "ANSI_colors.hpp"
#include "ast.hpp"
#include "driver.hpp"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << print_red("Invalid argument: argc = 2, argv[1] = name of file\n");
        return 1;
    }

    yy::Driver_t driver;
    ast::ast_t ast;

    try {
        driver.parse(argv[1], ast.root_);
    } catch (const char* error_message) {
        return 1;
    }

    try {
        ast.execute();
    } catch (const char* error_message) {
        std::cout << print_red(error_message) << "\n";
        return 1;
    }

    return 0;
}