#pragma once

#include "parser.tab.hh"
#include "node.hpp"
#include <fstream>
#include <FlexLexer.h>

namespace yy {
class Driver_t {
    FlexLexer* plex_;

public:
    Driver_t(FlexLexer* plex) : plex_(plex) {}

    parser::token_type yylex(parser::semantic_type* yylval) {
        parser::token_type tt = static_cast<parser::token_type>(plex_->yylex());
        switch (tt) {
            case yy::parser::token_type::NUMBER:
                yylval->as<int>() = std::stoi(plex_->YYText());
                break;

            case yy::parser::token_type::ID: {
                parser::semantic_type tmp;
                tmp.as<std::string>() = plex_->YYText();
                yylval->swap<std::string>(tmp);
                break;
            }

            default:
                break;
        }
        return tt;
    }

    bool parse(const char* file_name, node::node_t*& root) {
        std::ifstream input_file(file_name);
        plex_->switch_streams(input_file, std::cout);

        parser parser(this, root);
        bool res = parser.parse();
        return !res;
    }
};
}