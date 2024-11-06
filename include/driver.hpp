#pragma once

#include "parser.tab.hh"
#include "node.hpp"
#include "lexer.hpp"
#include <cstring>

namespace yy {

class Driver_t {
    Lexer_t lexer_;

public:
    void report_undecl_error(const std::string& variable) {
        lexer_.print_syntax_error(variable.length());
        std::cout << print_red("syntax error at " << lexer_.get_location()
                  << ": \"" << variable << "\" - undeclared variable\n");
        throw error_t{""};
    }

    parser::token_type yylex(parser::semantic_type* yylval, location* loc) {
        parser::token_type tt = static_cast<parser::token_type>(lexer_.yylex());
        switch (tt) {
            case yy::parser::token_type::ERR:
                lexer_.print_syntax_error(strlen(lexer_.YYText()));
                break;

            case yy::parser::token_type::NUMBER:
                yylval->as<int>() = std::stoi(lexer_.YYText());
                break;

            case yy::parser::token_type::ID: {
                parser::semantic_type tmp;
                tmp.as<std::string>() = lexer_.YYText();
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
        lexer_.init(file_name);
        std::ifstream input_file(file_name);
        lexer_.switch_streams(input_file, std::cout);

        parser parser(this, root);
        bool res = parser.parse();
        return !res;
    }
};
}