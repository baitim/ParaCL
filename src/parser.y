/*
Grammar:
    scope        -> statement; scope | empty
    statement    -> assignment | print

    print        -> print rvalue
    rvalue       -> lvalue | expression

    assignment   -> lvalue = expression
    lvalue       -> id
    expression   -> expression   bin_oper   expression_1 | expression_1
    expression_1 -> expression_2 bin_oper_1 expression_2 | expression_2
    expression_2 -> expression_2 bin_oper_2 number | number
*/

%language "c++"
%skeleton "lalr1.cc"
%defines
%define api.value.type variant

%code requires
{
    #include "node.hpp"
    using namespace node;
    #include <iostream>
    namespace yy {class Driver_t;}
}

%param { yy::Driver_t* driver }
%parse-param {node_t*& root}

%code
{
    #include "driver.hpp"
    namespace yy {
        parser::token_type yylex(parser::semantic_type* yylval, Driver_t* driver);
    }
}

%token
    PRINT

    EQUAL
    NEQUAL
    ELESS
    EGREATER
    LESS
    GREATER

    ASSIGN
    ADD
    SUB
    MUL
    DIV

    SCOLON
    ERR
;

%token <int>         NUMBER
%token <std::string> ID
%nterm <node_scope_t*> scope
%nterm <node_t*> statement
%nterm <node_t*> print
%nterm <node_t*> rvalue
%nterm <node_t*> assignment
%nterm <node_t*> lvalue
%nterm <node_t*> expression
%nterm <node_t*> expression_1
%nterm <node_t*> expression_2
%nterm <binary_operators_e> bin_oper
%nterm <binary_operators_e> bin_oper_1
%nterm <binary_operators_e> bin_oper_2

%code
{
    node_scope_t* current_scope = nullptr;
}

%start program

%%

program: scope { root = $1; }
;

scope: %empty          { $$ = new node_scope_t(); current_scope = $$; }
     | scope statement { $$ = $1; $$->add_statement($2); }
;

statement: assignment SCOLON { $$ = $1; }
         | print      SCOLON { $$ = $1; }
         | statement  SCOLON { $$ = $1; }
;

print: PRINT rvalue { $$ = new node_print_t($2); }
;

rvalue: lvalue     { $$ = $1; }
      | expression { $$ = $1; }
;

assignment: lvalue ASSIGN expression { $$ = new node_bin_op_t(binary_operators_e::ASSIGN, $1, $3); }
;

lvalue: ID {
                if(current_scope->find_variable($1)) {
                    $$ = current_scope->get_variable($1);
                } else {
                    $$ = new node_id_t();
                    current_scope->add_variable($1, $$);
                }
            }
;

expression  : expression   bin_oper   expression_1 { $$ = new node_bin_op_t($2, $1, $3); }
            | expression_1                         { $$ = $1; }
;

expression_1: expression_1 bin_oper_1 expression_2 { $$ = new node_bin_op_t($2, $1, $3); }
            | expression_2                         { $$ = $1; }
;

expression_2: expression_2 bin_oper_2 NUMBER { $$ = new node_bin_op_t($2, $1, new node_number_t($3)); }
            | NUMBER                         { $$ = new node_number_t($1); }
;

bin_oper   : EQUAL    { $$ = binary_operators_e::EQ; }
           | NEQUAL   { $$ = binary_operators_e::NE; }
           | ELESS    { $$ = binary_operators_e::LE; }
           | EGREATER { $$ = binary_operators_e::GE; }
           | LESS     { $$ = binary_operators_e::LT; }
           | GREATER  { $$ = binary_operators_e::GT; }
;

bin_oper_1 : ADD { $$ = binary_operators_e::ADD; }
           | SUB { $$ = binary_operators_e::SUB; }
;

bin_oper_2 : MUL { $$ = binary_operators_e::MUL; }
           | DIV { $$ = binary_operators_e::DIV; }
;

%%

namespace yy {
parser::token_type yylex(parser::semantic_type* yylval,
                         Driver_t* driver) {
    return driver->yylex(yylval);
}

void parser::error(const std::string&) {}
}