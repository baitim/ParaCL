#pragma once

#include "ANSI_colors.hpp"
#include <iostream>

#ifndef yyFlexLexer
#include <FlexLexer.h>
#endif

#include <fstream>
#include <sstream>

namespace yy {

    struct error_t final {
        std::string msg_;
        error_t(const char*        msg) : msg_(msg) {}
        error_t(const std::string& msg) : msg_(msg) {}
    };

    class Lexer_t : public yyFlexLexer {
        location loc_;
        std::string program_str_;
        
    private:
        std::string get_location2str() const {
            std::stringstream ss;
            ss << loc_;
            return ss.str();
        }

        std::pair<int, int> get_location2pair() const {
            std::string loc_str = get_location2str();
            size_t dot_location = loc_str.find('.');
            int line   = stoi(loc_str.substr(0, dot_location));
            int column = stoi(loc_str.substr(dot_location + 1));
            return std::make_pair(line - 1, column);
        };

        std::pair<std::string, int> get_current_line() const {
            const std::pair<int, int> location = get_location2pair();

            int line = 0;
            for (int i = 0, end = location.first; i < end; ++i)
                line = program_str_.find('\n', line + 1);
            line++;
            int end_of_line = program_str_.find('\n', line);

            return std::make_pair(program_str_.substr(line, end_of_line - line), location.second - 2);
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
                throw error_t{"can't open program file"};
            }
        }

        void print_syntax_error(int length) const {
            std::pair<std::string, int> line_info = get_current_line();
            std::string line = line_info.first;
            int loc    = line_info.second - length + 1;

            int error_loc_end = loc + length;

            std::cout << line.substr(0, loc)
                      << print_red(line.substr(loc, length))
                      << line.substr(error_loc_end, line.length()) << "\n";

            for (int i = 0, end = line.length(); i < end; ++i) {
                if (i >= loc && i < error_loc_end)
                    std::cout << print_red("^");
                else
                    std::cout << " ";
            }
            std::cout << "\n";
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