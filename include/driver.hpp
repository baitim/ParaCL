#pragma once

#include "parser.tab.hh"
#include "node.hpp"
#include "lexer.hpp"
#include <fstream>

namespace yy {

class Driver_t {
    Lexer_t plex_;

public:
    parser::token_type yylex(parser::semantic_type* yylval, location* loc) {
        parser::token_type tt = static_cast<parser::token_type>(plex_.yylex());
        switch (tt) {
            case yy::parser::token_type::NUMBER:
                yylval->as<int>() = std::stoi(plex_.YYText());
                break;

            case yy::parser::token_type::ID: {
                parser::semantic_type tmp;
                tmp.as<std::string>() = plex_.YYText();
                yylval->swap<std::string>(tmp);
                break;
            }

            default:
                break;
        }
        *loc = plex_.get_location();
        return tt;
    }

    bool parse(const std::string& file_name, node::node_t*& root) {
        std::ifstream input_file(file_name);
        plex_.switch_streams(input_file, std::cout);

        parser parser(this, root);
        bool res = parser.parse();
        return !res;
    }
};
}