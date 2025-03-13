#include "ParaCL/ast.hpp"
#include "ParaCL/driver.hpp"
#include "ParaCL/cmd.hpp"

int main(int argc, char* argv[]) try {
    paracl::cmd_data_t cmd_data;
    cmd_data.parse(argc, argv);

    std::string program_str = paracl::file2str(cmd_data.program_file());

    yy::driver_t driver;
    paracl::ast_t ast;
    driver.parse(cmd_data.program_file(), ast.buffer_, ast.root_, program_str);

    paracl::environments_t env{&(std::cout), &(std::cin), program_str};
    ast.analyze(env);

    if (!cmd_data.is_analyze_only())
        ast.execute(env);

} catch (const paracl::error_t& error) {
    std::cout << error.what() << '\n';
    return 1;
} catch (...) {
    std::cout << print_red("Unknown error\n");
    return 1;
}