#pragma once

#include "parser.tab.hh"
#include "ast.hpp"
#include "lexer.hpp"
#include <cstring>
#include <ranges>

namespace yy {

    namespace rng  = std::ranges;
    namespace view = rng::views;

    template <typename MsgT>
    concept error_str =
    std::is_constructible_v<std::string, MsgT> &&
    requires(std::ostream& os, MsgT msg) {
        { os << msg } -> std::same_as<std::ostream&>;
    };

    class error_t {
    protected:
        std::string msg_;

    private:
        template <error_str MsgT>
        std::string convert2colored(MsgT msg, bool is_colored) const {
            std::stringstream error;
            if (is_colored) error << print_red(msg);
            else            error << msg;
            return error.str();
        }

    public:
        error_t() {}
        template <error_str MsgT>
        error_t(MsgT msg, bool is_colored = false) : msg_(convert2colored(msg, is_colored)) {}

        virtual const char* what() const { return msg_.c_str(); }

        virtual ~error_t() {};
    };

    /*----------------------------------------------------------------------*/

    class parse_error_t : public error_t {
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

            error_line << line.substr(0, loc)
                        << print_red(line.substr(loc, length_))
                        << line.substr(loc + length_, line.length()) << "\n";

            for (int i : view::iota(0, static_cast<int>(line.length()))) {
                if (i >= loc && i < loc + length_)
                    error_line << print_red("^");
                else
                    error_line << " ";
            }
            error_line << "\n";
            return error_line.str();
        }

    public:
        parse_error_t(const location& loc, const std::string& program_str, int length)
        : loc_(loc), program_str_(program_str), length_(length) {}

        const location& get_loc() const { return loc_; }
    };

    /*----------------------------------------------------------------------*/

    class undecl_error_t final : public parse_error_t {
        std::string variable_;

    private:
        std::string get_info() const {
            std::stringstream description;
            description << parse_error_t::get_error_line();
            description << print_red("declaration error at " << parse_error_t::get_loc()
                        << ": \"" << variable_ << "\" - undeclared variable");

            return description.str();
        }

    public:
        undecl_error_t(const location& loc, const std::string& program_str, std::string_view variable)
        : parse_error_t(loc, program_str, variable.length()), variable_(variable) {
            error_t::msg_ = get_info();
        }
    };

    /*----------------------------------------------------------------------*/

    class syntax_error_t final : public parse_error_t {
        std::string token_;

    private:
        std::string get_info() const {
            std::stringstream description;
            description << parse_error_t::get_error_line();
            description << print_red("syntax error at " << parse_error_t::get_loc()
                        << ": \"" << token_ << "\" - token that breaks");

            return description.str();
        }

    public:
        syntax_error_t(const location& loc, const std::string& program_str, std::string_view token)
        : parse_error_t(loc, program_str, token.length()), token_(token) {
            error_t::msg_ = get_info();
        }
    };

}

namespace yy {

    class Driver_t final {
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
                throw error_t{"can't open program file", true};
            }
        }

        void report_undecl_error(const location& loc, std::string_view variable) const {
            throw undecl_error_t{loc, program_str_, variable};
        }

        void report_syntax_error(const location& loc) const {
            throw syntax_error_t{loc, program_str_, last_token_};
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

        bool parse(const std::string& file_name, node::buffer_t& buf, node::node_scope_t*& root) {
            init(file_name);
            std::ifstream input_file(file_name);
            lexer_.switch_streams(input_file, std::cout);

            parser parser(this, buf, root);
            bool res = parser.parse();
            return !res;
        }
    };
}