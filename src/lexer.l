%option c++

%{
#include "grammar.tab.hh"
%}

WS      [ \t\n\v]+
NUMBER  [1-9][0-9]*

%%

{WS}             // skip
"+"              return yy::parser::token_type::PLUS;
"-"              return yy::parser::token_type::MINUS;
"="              return yy::parser::token_type::EQUAL;
";"              return yy::parser::token_type::SCOLON;
{NUMBER}         return yy::parser::token_type::NUMBER;
.                return yy::parser::token_type::ERR;

%%

// nothing