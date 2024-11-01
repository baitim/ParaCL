%option c++

%{
#include "parser.tab.hh"
%}

WS       [ \t\n\v]+
NUMBER   [1-9][0-9]*
ID       [a-zA-Z_][a-zA-Z0-9_]*

%%

{WS}             // skip
{NUMBER}         {std::cout << "number <" << yytext    << ">\n"; return yy::parser::token_type::NUMBER; }
{ID}             {std::cout << "    id <" << yytext    << ">\n"; return yy::parser::token_type::ID; }
"="              {std::cout << "     = <" << yytext[0] << ">\n"; return yy::parser::token_type::ASSIGN; }
"+"              {std::cout << "     + <" << yytext[0] << ">\n"; return yy::parser::token_type::PLUS; }
"-"              {std::cout << "     - <" << yytext[0] << ">\n"; return yy::parser::token_type::MINUS; }
";"              {std::cout << "     ; <" << yytext[0] << ">\n"; return yy::parser::token_type::SCOLON; }
.                {std::cout << "     . <" << yytext[0] << ">\n"; return yy::parser::token_type::ERR; }

%%

// nothing