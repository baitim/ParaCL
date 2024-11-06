#pragma once

#include "parser.tab.hh"
#include "node.hpp"
#include "lexer.hpp"
#include <cstring>

namespace yy {

    inline std::string get_location2str(const location& loc) {
        std::stringstream ss;
        ss << loc;
        return ss.str();
    }

    inline std::pair<int, int> get_location2pair(const location& loc) {
        std::string loc_str = get_location2str(loc);
        size_t dot_location = loc_str.find('.');
        int line   = stoi(loc_str.substr(0, dot_location));
        int column = stoi(loc_str.substr(dot_location + 1));
        return std::make_pair(line - 1, column);
    };

    inline std::pair<std::string, int> get_current_line(const location& loc, const std::string& program_str) {
        const std::pair<int, int> location = get_location2pair(loc);

        int line = 0;
        for (int i = 0, end = location.first; i < end; ++i)
            line = program_str.find('\n', line + 1);
        line++;
        int end_of_line = program_str.find('\n', line);

        if (program_str[line - 1] != '\n') {
            line = 0;
            end_of_line = program_str.length();
        }

        return std::make_pair(program_str.substr(line, end_of_line - line), location.second - 2);
    };

    inline void print_syntax_error(const location& loc_, const std::string& program_str, int length) {
        std::pair<std::string, int> line_info = get_current_line(loc_, program_str);
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

    class Driver_t {
        Lexer_t lexer_;
        std::string program_str_;
        std::string last_token_;

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

        void report_undecl_error(const location& loc, const std::string& variable) {
            print_syntax_error(loc, program_str_, variable.length());
            std::cout << print_red("undecl error at " << loc
                      << ": \"" << variable << "\" - undeclared variable\n");
            throw error_t{""};
        }

        void report_syntax_error(const location& loc) {
            print_syntax_error(loc, program_str_, last_token_.length());
            std::cout << print_red("syntax error at " << loc
                      << ": \"" << last_token_ << "\" - token that breaks\n");
            throw error_t{""};
        }

        parser::token_type yylex(parser::semantic_type* yylval, location* loc) {
            parser::token_type tt = static_cast<parser::token_type>(lexer_.yylex());
            last_token_ = lexer_.YYText();
            switch (tt) {
                case yy::parser::token_type::NUMBER:
                    yylval->as<int>() = std::stoi(last_token_);
                    break;

                case yy::parser::token_type::ID: {
                    parser::semantic_type tmp;
                    tmp.as<std::string>() = last_token_;
                    yylval->swap<std::string>(tmp);
                    break;
                }

                default:
                    break;
            }
            *loc = lexer_.get_location();
            return tt;
        }

        bool parse(const std::string& file_name, node::node_t*& root) {
            init(file_name);
            std::ifstream input_file(file_name);
            lexer_.switch_streams(input_file, std::cout);

            parser parser(this, root);
            bool res = parser.parse();
            return !res;
        }
    };
}