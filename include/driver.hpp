#pragma once

#include "parser.tab.hh"
#include "node.hpp"
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

    bool parse(node::node_t*& root) {
        parser parser(this, root);
        bool res = parser.parse();
        return !res;
    }
};
}