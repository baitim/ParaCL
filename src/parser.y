/*
Grammar:
    statements -> statement; statements | empty
    statement  -> assignment
    assignment -> lvalue = statement
    expression -> expression + number | expression - number | number
*/

%language "c++"
%skeleton "lalr1.cc"
%defines
%define api.value.type variant
%param {yy::Driver_t* driver}

%code requires
{
#include "node.hpp"
using namespace node;
#include <iostream>
namespace yy {class Driver_t;}
}

%code
{
#include "driver.hpp"
namespace yy {
    parser::token_type yylex(parser::semantic_type* yylval, Driver_t* driver);
}
}

%token
 ASSIGN
 PLUS
 MINUS
 SCOLON
 ERR
;

%token <int>         NUMBER
%token <std::string> ID
%nterm statements
%nterm <node_t*> lvalue
%nterm <node_t*> expression
%nterm <node_t*> assignment

%start program

%%

program: statements
;

statements: statement SCOLON statements
          | %empty
;

statement: assignment

assignment: lvalue ASSIGN expression { $$ = new node_bin_op_t(binary_operators_e::ASSIGN, $1, $3); }
;

lvalue: ID { $$ = new node_id_t($1); }
;

expression: expression PLUS  NUMBER { $$ = new node_bin_op_t(binary_operators_e::ADD,
                                                             $1, new node_number_t($3)); }
          | expression MINUS NUMBER { $$ = new node_bin_op_t(binary_operators_e::SUB,
                                                             $1, new node_number_t($3)); }
          | NUMBER                  { $$ = new node_number_t($1); }
;

%%

namespace yy {
parser::token_type yylex(parser::semantic_type* yylval,
                         Driver_t* driver) {
    return driver->yylex(yylval);
}

void parser::error(const std::string&) {}
}