/*
Grammar:
    scope        -> scope ustatement | scope; | empty
    ustatement   -> statement | rvalue;                 // universal
    rstatement   -> print | assignment                  //    return
     statement   -> fork  | loop                        // no return

    fork         -> if condition body | if condition body else body
    loop         -> while condition body

    condition    -> '(' rvalue ')'
    body         -> '{' scope '}' | lghost_scope ustatement rghost_scope;

    print        -> print rvalue
    assignment   -> lvalue = rvalue

    rvalue         -> rstatement | expression_lgc
    expression_lgc -> expression_lgc bin_oper_lgc expression_cmp | expression_cmp
    expression_cmp -> expression_pls bin_oper_cmp expression_pls | expression_pls
    expression_pls -> expression_mul bin_oper_pls expression_mul | expression_mul
    expression_mul -> expression_mul bin_oper_mul terminal       | terminal
    terminal       -> '(' rvalue ')' | lvalue | number | ? | un_oper terminal
    lvalue         -> id
*/

%language "c++"
%skeleton "lalr1.cc"
%defines
%define api.value.type variant
%locations
%define api.location.file "location.hh"

%code requires
{
    #include "node.hpp"
    using namespace node;
    #include <stack>
    namespace yy {class Driver_t;}
}

%param       { yy::Driver_t* driver }
%parse-param { node::buffer_t& buf }
%parse-param { node::node_t*&  root }

%code
{
    #include "driver.hpp"
    namespace yy {
        parser::token_type yylex(parser::semantic_type* yylval,
                                 location* loc,
                                 Driver_t* driver);
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

    OR
    AND
    NOT

    ASSIGN
    ADD
    SUB
    MUL
    DIV
    MOD

    SCOLON
    ERR
;

%precedence "then"
%precedence FORK2

%token <int> NUMBER
%token <std::string> ID
%nterm <node_scope_t*> scope
%nterm <node_scope_t*> lghost_scope
%nterm rghost_scope
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
%nterm <node_t*> expression_lgc
%nterm <node_t*> expression_cmp
%nterm <node_t*> expression_pls
%nterm <node_t*> expression_mul
%nterm <node_t*> terminal
%nterm <std::string> lvalue
%nterm <binary_operators_e> bin_oper_lgc
%nterm <binary_operators_e> bin_oper_cmp
%nterm <binary_operators_e> bin_oper_pls
%nterm <binary_operators_e> bin_oper_mul
%nterm <unary_operators_e> un_oper

%code
{
    std::stack<node_scope_t*> scopes;
    node_scope_t* current_scope = nullptr;

    void drill_down_to_scope(node_scope_t* scope) {
        if (!scopes.empty())
            scope->copy_variables(scopes.top()->get_variables());
        scopes.push(scope);
        current_scope = scope;
    }

    void lift_up_from_scope() {
        scopes.pop();
        current_scope = scopes.top();
    }
}

%start program

%%

program: scope { root = $1; }
;

scope: %empty           { $$ = buf.add_node(node_scope_t{current_scope}); drill_down_to_scope($$); }
     | scope ustatement { $$ = $1; $$->add_statement($2); }
     | scope SCOLON     { $$ = $1; }
;

ustatement: statement      { $$ = $1; }
          | rvalue SCOLON  { $$ = $1; }
;

rstatement: print      { $$ = $1; }
          | assignment { $$ = $1; }
;

statement: fork       { $$ = $1; }
         | loop       { $$ = $1; }
;

fork: FORK1 condition body %prec "then" { $$ = buf.add_node(node_fork_t{$2, $3, nullptr}); }
    | FORK1 condition body FORK2 body   { $$ = buf.add_node(node_fork_t{$2, $3, $5}); }
;

loop: LOOP condition body { $$ = buf.add_node(node_loop_t{$2, $3}); }
;

condition: LBRACKET rvalue RBRACKET { $$ = $2; }
;

body: LSCOPE scope RSCOPE                   { $$ = $2; lift_up_from_scope(); }
    | lghost_scope ustatement rghost_scope  { $1->add_statement($2); $$ = $1; }
    | SCOLON                                { $$ = nullptr; }
;

lghost_scope: %empty { $$ = buf.add_node(node_scope_t{current_scope}); drill_down_to_scope($$); }
rghost_scope: %empty { lift_up_from_scope(); }

print: PRINT rvalue { $$ = buf.add_node(node_print_t{$2}); }
;

assignment: lvalue ASSIGN rvalue {
                                    node_t* lvalue_node;
                                    if (current_scope->find_variable($1)) {
                                        lvalue_node = current_scope->get_node($1);
                                    } else {
                                        lvalue_node = buf.add_node(node_id_t{});
                                        current_scope->add_variable($1, lvalue_node);
                                    }
                                    $$ = buf.add_node(node_bin_op_t{binary_operators_e::ASSIGN, lvalue_node, $3});
                                 }
;

rvalue: rstatement     { $$ = $1; }
      | expression_lgc { $$ = $1; }
;

expression_lgc: expression_lgc bin_oper_lgc expression_cmp { $$ = buf.add_node(node_bin_op_t{$2, $1, $3}); }
              | expression_cmp                             { $$ = $1; }
;

expression_cmp: expression_cmp bin_oper_cmp expression_pls { $$ = buf.add_node(node_bin_op_t{$2, $1, $3}); }
              | expression_pls                             { $$ = $1; }
;

expression_pls: expression_pls bin_oper_pls expression_mul { $$ = buf.add_node(node_bin_op_t{$2, $1, $3}); }
              | expression_mul                             { $$ = $1; }
;

expression_mul: expression_mul bin_oper_mul terminal       { $$ = buf.add_node(node_bin_op_t{$2, $1, $3}); }
              | terminal                                   { $$ = $1; }
;

terminal: LBRACKET rvalue RBRACKET  { $$ = $2; }
        | NUMBER                    { $$ = buf.add_node(node_number_t{$1}); }
        | INPUT                     { $$ = buf.add_node(node_input_t{}); }
        | un_oper terminal          { $$ = buf.add_node(node_un_op_t($1, $2)); }
        | ID                        {
                                        if (!current_scope->find_variable($1))
                                            driver->report_undecl_error(@1, $1);
                                        $$ = current_scope->get_node($1);
                                    }
;

lvalue: ID { $$ = $1; }
;

bin_oper_lgc : OR   { $$ = binary_operators_e::OR; }
             | AND  { $$ = binary_operators_e::AND; }
;

bin_oper_cmp : EQUAL    { $$ = binary_operators_e::EQ; }
             | NEQUAL   { $$ = binary_operators_e::NE; }
             | ELESS    { $$ = binary_operators_e::LE; }
             | EGREATER { $$ = binary_operators_e::GE; }
             | LESS     { $$ = binary_operators_e::LT; }
             | GREATER  { $$ = binary_operators_e::GT; }
;

bin_oper_pls : ADD { $$ = binary_operators_e::ADD; }
             | SUB { $$ = binary_operators_e::SUB; }
;

bin_oper_mul : MUL { $$ = binary_operators_e::MUL; }
             | DIV { $$ = binary_operators_e::DIV; }
             | MOD { $$ = binary_operators_e::MOD; }
;

un_oper : ADD  { $$ = unary_operators_e::ADD; }
        | SUB  { $$ = unary_operators_e::SUB; }
        | NOT  { $$ = unary_operators_e::NOT; }
;

%%

namespace yy {
parser::token_type yylex(parser::semantic_type* yylval,
                         location* loc,
                         Driver_t* driver) {
    return driver->yylex(yylval, loc);
}

void parser::error(const location_type& loc, const std::string& message) {
    driver->report_syntax_error(loc);
    throw error_t{""};
}
}