#include "ast.hpp"
#include "driver.hpp"
#include "cmd.hpp"

int main(int argc, char* argv[]) {
    cmd::cmd_data_t cmd_data;
    try {
        cmd_data.parse(argc, argv);
    } catch (const common::error_t& error) {
        std::cout << error.what() << "\n";
        return 1;
    }

    yy::driver_t driver;
    ast::ast_t ast;
    try {
        driver.parse(cmd_data.program_file(), ast.buffer_, ast.root_);
    } catch (const common::error_t& error) {
        std::cout << error.what() << "\n";
        return 1;
    }

    environments::environments_t env{std::cout, std::cin, cmd_data.is_analyzing()};
    try {
        ast.execute(env);
    } catch (const node::error_execute_t& error) {
        std::cout << print_red(error.what()) << "\n";
        return 1;
    }

    return 0;
}