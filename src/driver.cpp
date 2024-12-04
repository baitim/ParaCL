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
        driver.parse(argv[1], ast.buffer_, ast.root_);
    } catch (const yy::error_t& error) {
        std::cout << error.what() << "\n";
        return 1;
    }

    environments::environments_t env{std::cout, std::cin};

    try {
        ast.execute(env);
    } catch (const node::error_t& error) {
        std::cout << print_red(error.what()) << "\n";
        return 1;
    }

    return 0;
}