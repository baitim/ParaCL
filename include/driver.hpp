#pragma once

#include "parser.tab.hh"
#include "ast.hpp"
#include "lexer.hpp"
#include <string>

namespace yy {
    class error_parse_t : public common::error_t {
        location loc_;
        std::string program_str_;
        int length_;

    private:
        static std::string get_location2str(const location& loc) {
            std::stringstream ss;
            ss << loc;
            return ss.str();
        }

        static std::pair<int, int> get_location2pair(const location& loc) {
            std::string loc_str = get_location2str(loc);
            size_t dot_location = loc_str.find('.');
            int line   = stoi(loc_str.substr(0, dot_location));
            int column = stoi(loc_str.substr(dot_location + 1));
            return std::make_pair(line - 1, column);
        };

        static std::pair<std::string, int> get_current_line(const location& loc,
                                                            const std::string& program_str) {
            const std::pair<int, int> location = get_location2pair(loc);

            int line = 0;
            for ([[maybe_unused]]int _ : view::iota(0, location.first))
                line = program_str.find('\n', line + 1);

            if (line > 0)
                line++;

            int end_of_line = program_str.find('\n', line);
            if (end_of_line == -1)
                end_of_line = program_str.length();

            return std::make_pair(program_str.substr(line, end_of_line - line), location.second - 2);
        };

    protected:
        std::string get_error_line() const {
            std::stringstream error_line;

            std::pair<std::string, int> line_info = get_current_line(loc_, program_str_);
            std::string line = line_info.first;
            int loc = line_info.second - length_ + 1;
            const int line_length = line.length();

            error_line << line.substr(0, loc)
                        << print_red(line.substr(loc, length_))
                        << line.substr(loc + length_, line_length) << "\n";

            for (int i : view::iota(0, line_length)) {
                if (i >= loc && i < loc + length_)
                    error_line << print_red("^");
                else
                    error_line << " ";
            }
            error_line << "\n";
            return error_line.str();
        }

    public:
        error_parse_t(const location& loc, const std::string& program_str, int length)
        : loc_(loc), program_str_(program_str), length_(length) {}

        const location& get_loc() const { return loc_; }
    };

    /*----------------------------------------------------------------------*/

    class error_undecl_t : public error_parse_t {
        std::string variable_;

    private:
        std::string get_info() const {
            std::stringstream description;
            description << error_parse_t::get_error_line();
            description << print_red("declaration error at " << error_parse_t::get_loc()
                        << ": \"" << variable_ << "\" - undeclared variable");

            return description.str();
        }

    public:
        error_undecl_t(const location& loc, const std::string& program_str, std::string_view variable)
        : error_parse_t(loc, program_str, variable.length()), variable_(variable) {
            common::error_t::msg_ = get_info();
        }
    };

    /*----------------------------------------------------------------------*/

    class error_syntax_t : public error_parse_t {
        std::string token_;

    private:
        std::string get_info() const {
            std::stringstream description;
            description << error_parse_t::get_error_line();
            description << print_red("syntax error at " << error_parse_t::get_loc()
                        << ": \"" << token_ << "\" - token that breaks");

            return description.str();
        }

    public:
        error_syntax_t(const location& loc, const std::string& program_str, std::string_view token)
        : error_parse_t(loc, program_str, token.length()), token_(token) {
            common::error_t::msg_ = get_info();
        }
    };

}

namespace yy {

    class driver_t final {
        lexer_t lexer_;
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
                throw common::error_t{"can't open program file", true};
            }
        }

        void report_undecl_error(const location& loc, std::string_view variable) const {
            throw error_undecl_t{loc, program_str_, variable};
        }

        void report_syntax_error(const location& loc) const {
            throw error_syntax_t{loc, program_str_, last_token_};
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

        bool parse(const std::string& file_name, node::buffer_t& buf,
                   node::node_scope_t*& root, environments::environments_t& parse_env) {
            init(file_name);
            std::ifstream input_file(file_name);
            lexer_.switch_streams(input_file, std::cout);

            parser parser(this, buf, root, parse_env);
            bool res = parser.parse();
            return !res;
        }
    };
}