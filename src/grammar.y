/*
Grammar:
    EQL -> EQ; EQL | empty
    EQ  -> E = E
    E -> number | E + number | E - number
*/

%language "c++"
%skeleton "lalr1.cc"
%defines
%define api.value.type variant
%param {yy::Driver_t* driver}

%code requires
{
#include <iostream>
#include <string>
namespace yy {class Driver_t;}
}

%code
{
#include "driver.hpp"
namespace yy {
    parser::token_type yylex(parser::semantic_type* yylval,
                             Driver_t* driver);
}
}

%token
 EQUAL  "="
 PLUS   "+"
 MINUS  "-"
 SCOLON ";"
 ERR
;

%token <int> NUMBER
%nterm <int> equals
%nterm <int> expr

%left '+' '-'

%start program

%%

program: eqlist
;

eqlist: equals SCOLON eqlist
      | %empty
;

equals: expr EQUAL expr     {
                                $$ = ($1 == $3);
                                std::cout << "Checking: " << $1 << " vs " << $3 << "; Result: " << $$ << "\n";
                            }
;

expr: expr PLUS NUMBER      { $$ = $1 + $3; }
    | expr MINUS NUMBER     { $$ = $1 - $3; }
    | NUMBER                { $$ = $1; }
;

%%

namespace yy {
parser::token_type yylex(parser::semantic_type* yylval,
                         Driver_t* driver) {
    return driver->yylex(yylval);
}

void parser::error(const std::string&) {}
}