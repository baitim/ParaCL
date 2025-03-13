#pragma once

#include "ANSI_colors.hpp"
#include <sstream>
#include <fstream>

namespace paracl {

    class error_t : public std::runtime_error {
    public:
        error_t(std::string msg) : std::runtime_error(msg) {}
    };

    inline std::string file2str(const std::string& file_name) {
        std::ifstream input_file(file_name);
        if (input_file.is_open()) {
            std::stringstream sstr;
            sstr << input_file.rdbuf();
            input_file.close();
            return sstr.str();
        }
        throw error_t{str_red(std::string{"can't open program file: "} + file_name)};
    }
}