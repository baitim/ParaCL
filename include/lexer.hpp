#pragma once

#include "ANSI_colors.hpp"

#ifndef yyFlexLexer
#include <FlexLexer.h>
#endif

#include <cstring>
#include <fstream>
#include <sstream>

namespace yy {
class Lexer_t : public yyFlexLexer {
    location loc_;
    std::string program_str_;

    std::string get_location_str() {
        std::stringstream ss;
        ss << loc_;
        return ss.str();
    }

    std::pair<int, int> get_current_position() {
        std::string loc_str = get_location_str();
        size_t dot_posisition = loc_str.find('.');
        int line   = stoi(loc_str.substr(0, dot_posisition));
        int column = stoi(loc_str.substr(dot_posisition + 1));
        return std::make_pair(line - 1, column);
    };

    std::pair<std::string, int> get_current_position_line() {
        std::pair<int, int> position = get_current_position();

        int line = 0;
        for (int i = 0, end = position.first; i < end; ++i)
            line = program_str_.find('\n', line + 1);
        line++;
        int end_of_line = program_str_.find('\n', line);

        return std::make_pair(program_str_.substr(line, end_of_line - line), position.second - 2);
    };

public:
    void init(const std::string& file_name) {
        std::ifstream input_file(file_name);
        if (input_file.is_open()) {
            std::stringstream sstr;
            sstr << input_file.rdbuf();
            program_str_ = sstr.str();
            input_file.close();
        } else {
            throw paracl::error_t{"can't open program file"};
        }
    }

    void on_error() {
        std::pair<std::string, int> current_line_info = get_current_position_line();
        std::string line     = current_line_info.first;
        int         position = current_line_info.second;
    
        std::cout << line.substr(0, position)
                  << print_red(line[position])
                  << line.substr(position + 1, line.length()) << "\n";

        for (int i = 0; i < position; ++i)
            std::cout << " ";
        std::cout << print_red("^") << "\n";
    }
    
    void update_new_token() {
        loc_.columns(yyleng);
        loc_.step();
    }

    void update_new_line() {
        loc_.lines(yyleng);
        loc_.step();
    }

    location get_location() const { return loc_; }

    int yylex() override;
};
}