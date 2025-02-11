#include "ast.hpp"
#include "driver.hpp"
#include "cmd.hpp"

int main(int argc, char* argv[]) {
    cmd::cmd_data_t cmd_data;
    try {
        cmd_data.parse(argc, argv);
    } catch (const common::error_t& error) {
        return (std::cout << error.what() << "\n", 1);
    }

    std::string program_str;
    try {
        program_str = common::file2str(cmd_data.program_file());
    } catch (const common::error_t& error) {
        return (std::cout << error.what() << "\n", 1);
    }

    yy::driver_t driver;
    ast::ast_t ast;
    try {
        driver.parse(cmd_data.program_file(), ast.buffer_, ast.root_, program_str);
    } catch (const common::error_t& error) {
        return (std::cout << error.what() << "\n", 1);
    }

    environments::environments_t env{&(std::cout), &(std::cin), program_str};
    try {
        ast.analyze(env);
    } catch (const common::error_t& error) {
        return (std::cout << error.what() << "\n", 1);
    }

    if (!cmd_data.is_analyze_only()) try {
        ast.execute(env);
    } catch (const common::error_t& error) {
        return (std::cout << error.what() << "\n", 1);
    }

    return 0;
}