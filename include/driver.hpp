#pragma once

#include "parser.tab.hh"
#include "ast.hpp"
#include "lexer.hpp"

namespace yy {
    inline location_t make_loc(const location& loc, int len) {
        std::stringstream ss;
        ss << loc;
        std::string loc_str = ss.str();

        size_t dot_location = loc_str.find('.');
        int row = stoi(loc_str.substr(0, dot_location));
        int col = stoi(loc_str.substr(dot_location + 1));
        return {row - 1, col, len};
    };

    /* ----------------------------------------------------- */

    class error_syntax_t : public error_location_t {
    public:
        error_syntax_t(const location_t& loc, std::string_view program_str, const std::string& token)
        : error_location_t(loc, program_str, str_red("syntax error: \"" + token + "\" - token that breaks")) {}
    };

    /* ----------------------------------------------------- */

    class driver_t final {
        lexer_t lexer_;
        std::string_view program_str_;
        std::string last_token_;

    public:
        void report_syntax_error(const location& loc) const {
            throw error_syntax_t{
                make_loc(loc, last_token_.size()),
                program_str_,
                last_token_
            };
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
                   node::node_scope_t*& root, std::string_view program_str) {
            program_str_ = program_str;
            std::ifstream input_file(file_name);
            lexer_.switch_streams(input_file, std::cout);

            parser parser(this, buf, root);
            bool res = parser.parse();
            return !res;
        }
    };
}