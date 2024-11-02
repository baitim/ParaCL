/*
Grammar:
    scope        -> scope ustatement | scope; | empty
    ustatement   -> statement | rstatement;             // universal
    rstatement   -> print | assignment                  //    return
     statement   -> fork  | loop                        // no return

    fork         -> if condition body else body | if condition body
    loop         -> while condition body

    condition    -> '(' rvalue ')'
    body         -> '{' scope '}' | ustatement;

    print        -> print rvalue
    assignment   -> lvalue = rvalue

    rvalue       -> rstatement | expression
    expression   -> expression   bin_oper   expression_1 | expression_1
    expression_1 -> expression_2 bin_oper_1 expression_2 | expression_2
    expression_2 -> expression_2 bin_oper_2 terminal     | terminal
    terminal     -> '(' expression ')' | lvalue | number | ? | bin_oper_1 terminal
    lvalue       -> id
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
    #include <stack>
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
    INPUT
    FORK1
    FORK2
    LOOP

    LBRACKET
    RBRACKET
    LSCOPE
    RSCOPE

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

%token <int> NUMBER
%token <std::string> ID
%nterm <node_scope_t*> scope
%nterm <node_t*> ustatement
%nterm <node_t*> rstatement
%nterm <node_t*>  statement
%nterm <node_t*> fork
%nterm <node_t*> loop
%nterm <node_t*> condition
%nterm <node_t*> body
%nterm <node_t*> print
%nterm <node_t*> assignment
%nterm <node_t*> rvalue
%nterm <node_t*> expression
%nterm <node_t*> expression_1
%nterm <node_t*> expression_2
%nterm <node_t*> terminal
%nterm <node_t*> lvalue
%nterm <binary_operators_e> bin_oper
%nterm <binary_operators_e> bin_oper_1
%nterm <binary_operators_e> bin_oper_2

%code
{
    std::stack<node_scope_t*> scopes;
    node_scope_t* current_scope = nullptr;
}

%start program

%%

program: scope { root = $1; }
;

scope: %empty           {
                            $$ = new node_scope_t(current_scope);
                            if (!scopes.empty())
                                $$->copy_variables(scopes.top()->get_variables());
                            scopes.push($$);
                            current_scope = $$;
                        }
     | scope ustatement { $$ = $1; $$->add_statement($2); }
     | scope SCOLON     { $$ = $1; }
;

ustatement:  statement        { $$ = $1; }
          | rstatement SCOLON { $$ = $1; }
;

rstatement: print      { $$ = $1; }
          | assignment { $$ = $1; }
;

statement: fork       { $$ = $1; }
         | loop       { $$ = $1; }
;

fork: FORK1 condition body FORK2 body { $$ = new node_fork_t($2, $3, $5); }
    | FORK1 condition body            { $$ = new node_fork_t($2, $3, nullptr); }
;

loop: LOOP condition body { $$ = new node_loop_t($2, $3); }
;

condition: LBRACKET rvalue RBRACKET { $$ = $2; }
;

body: LSCOPE scope RSCOPE { $$ = $2; scopes.pop(); }
    | ustatement          { $$ = $1; }
    | SCOLON              { $$ = nullptr; }
;

print: PRINT rvalue { $$ = new node_print_t($2); }
;

assignment: lvalue ASSIGN rvalue { $$ = new node_bin_op_t(binary_operators_e::ASSIGN, $1, $3); }
;

rvalue: rstatement { $$ = $1; }
      | expression { $$ = $1; }
;

expression  : expression   bin_oper   expression_1 { $$ = new node_bin_op_t($2, $1, $3); }
            | expression_1                         { $$ = $1; }
;

expression_1: expression_1 bin_oper_1 expression_2 { $$ = new node_bin_op_t($2, $1, $3); }
            | expression_2                         { $$ = $1; }
;

expression_2: expression_2 bin_oper_2 terminal     { $$ = new node_bin_op_t($2, $1, $3); }
            | terminal                             { $$ = $1; }
;

terminal: LBRACKET expression RBRACKET   { $$ = $2; }
        | lvalue                         { $$ = $1; }
        | NUMBER                         { $$ = new node_number_t($1); }
        | INPUT                          { $$ = new node_input_t(); }
        | bin_oper_1 terminal            { $$ = new node_bin_op_t($1, new node_number_t(0), $2); }
;

lvalue: ID {
                if (current_scope->find_variable($1)) {
                    $$ = current_scope->get_node($1);
                } else {
                    $$ = new node_id_t();
                    current_scope->add_variable($1, $$);
                }
            }
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