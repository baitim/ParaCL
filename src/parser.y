/*
Grammar:
    program      -> statements
    statements   -> statements statement | statements; | statements scope | empty
    scope        -> '{' statements '}'
    statement    -> fork  | loop | instruction
    instruction  -> expression;
    expression   -> print | assignment | expression_lgc

    fork         -> if condition body | if condition body else body
    loop         -> while condition body

    condition    -> '(' expression ')'
    body         -> scope | lghost_scope statement rghost_scope | ;

    print        -> print expression
    assignment   -> lvalue = expression

    expression_lgc -> expression_lgc bin_oper_lgc expression_cmp | expression_cmp
    expression_cmp -> expression_cmp bin_oper_cmp expression_pls | expression_pls
    expression_pls -> expression_pls bin_oper_pls expression_mul | expression_mul
    expression_mul -> expression_mul bin_oper_mul terminal       | terminal
    terminal       -> '(' expression ')' | number | ? | un_oper terminal | id
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
%parse-param { node::node_scope_t*& root }

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
    IF
    ELSE
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

%precedence THEN
%precedence ELSE

%token <int>                NUMBER
%token <std::string>        ID

%nterm <node_scope_t*>      statements
%nterm <node_scope_t*>      scope
%nterm <node_scope_t*>      body
%nterm <node_scope_t*>      lghost_scope
%nterm                      rghost_scope

%nterm <node_statement_t*>  statement
%nterm <node_statement_t*>  instruction
%nterm <node_expression_t*> expression

%nterm <node_statement_t*>  fork
%nterm <node_statement_t*>  loop
%nterm <node_expression_t*> condition

%nterm <node_expression_t*> print
%nterm <node_expression_t*> assignment

%nterm <std::string>        lvalue
%nterm <node_expression_t*> terminal

%nterm <node_expression_t*> expression_lgc
%nterm <node_expression_t*> expression_cmp
%nterm <node_expression_t*> expression_pls
%nterm <node_expression_t*> expression_mul

%nterm <binary_operators_e> bin_oper_lgc
%nterm <binary_operators_e> bin_oper_cmp
%nterm <binary_operators_e> bin_oper_pls
%nterm <binary_operators_e> bin_oper_mul

%nterm <unary_operators_e>  un_oper

%code
{
    std::stack<node_scope_t*> scopes_stack;
    node_scope_t* current_scope = nullptr;

    void drill_down_to_scope(node_scope_t* scope) {
        scopes_stack.push(scope);
        current_scope = scope;
    }

    void lift_up_from_scope() {
        scopes_stack.pop();
        current_scope = scopes_stack.top();
    }
}

%start program

%%

program: statements { root = $1; }
;

statements: %empty                 { $$ = buf.add_node<node_scope_t>(current_scope); drill_down_to_scope($$); }
          | statements statement   { $$ = $1; $$->add_statement($2); }
          | statements SCOLON      { $$ = $1; }
          | statements scope       { $$ = $1; $$->add_statement($2); lift_up_from_scope(); }
;

scope: LSCOPE statements RSCOPE { $$ = $2; }

statement: fork         { $$ = $1; }
         | loop         { $$ = $1; }
         | instruction  { $$ = $1; }
;

instruction: expression SCOLON { $$ = buf.add_node<node_instruction_t>($1); }
;

expression: print          { $$ = $1; }
          | assignment     { $$ = $1; }
          | expression_lgc { $$ = $1; }
;

fork: IF condition body %prec THEN  { 
                                        $$ = buf.add_node<node_fork_t>(
                                                $2, $3, buf.add_node<node_scope_t>(current_scope));
                                    }
    | IF condition body ELSE body   { $$ = buf.add_node<node_fork_t>($2, $3, $5); }
;

loop: LOOP condition body { $$ = buf.add_node<node_loop_t>($2, $3); }
;

condition: LBRACKET expression RBRACKET { $$ = $2; }
;

body: scope                                 { $$ = $1; lift_up_from_scope(); }
    | lghost_scope statement rghost_scope   { $1->add_statement($2); $$ = $1; }
    | SCOLON                                { $$ = buf.add_node<node_scope_t>(current_scope); }
;

lghost_scope: %empty { $$ = buf.add_node<node_scope_t>(current_scope); drill_down_to_scope($$); }
rghost_scope: %empty { lift_up_from_scope(); }

print: PRINT expression { $$ = buf.add_node<node_print_t>($2); }
;

assignment: lvalue ASSIGN expression {
                                        node_var_t* lvalue_node = current_scope->get_node($1);
                                        if (!lvalue_node) {
                                            lvalue_node = buf.add_node<node_var_t>($1);
                                            current_scope->add_variable(lvalue_node);
                                        }
                                        $$ = buf.add_node<node_assign_t>(lvalue_node, $3);
                                     }
;

expression_lgc: expression_lgc bin_oper_lgc expression_cmp { $$ = buf.add_node<node_bin_op_t>($2, $1, $3); }
              | expression_cmp                             { $$ = $1; }
;

expression_cmp: expression_cmp bin_oper_cmp expression_pls { $$ = buf.add_node<node_bin_op_t>($2, $1, $3); }
              | expression_pls                             { $$ = $1; }
;

expression_pls: expression_pls bin_oper_pls expression_mul { $$ = buf.add_node<node_bin_op_t>($2, $1, $3); }
              | expression_mul                             { $$ = $1; }
;

expression_mul: expression_mul bin_oper_mul terminal       { $$ = buf.add_node<node_bin_op_t>($2, $1, $3); }
              | terminal                                   { $$ = $1; }
;

terminal: LBRACKET expression RBRACKET  { $$ = $2; }
        | NUMBER                        { $$ = buf.add_node<node_number_t>($1); }
        | INPUT                         { $$ = buf.add_node<node_input_t>(); }
        | un_oper terminal              { $$ = buf.add_node<node_un_op_t>($1, $2); }
        | ID                            {
                                            $$ = current_scope->get_node($1);
                                            if (!$$)
                                                driver->report_undecl_error(@1, $1);
                                        }
;

lvalue: ID { $$ = $1; }
;

bin_oper_lgc: OR   { $$ = binary_operators_e::OR; }
            | AND  { $$ = binary_operators_e::AND; }
;

bin_oper_cmp: EQUAL    { $$ = binary_operators_e::EQ; }
            | NEQUAL   { $$ = binary_operators_e::NE; }
            | ELESS    { $$ = binary_operators_e::LE; }
            | EGREATER { $$ = binary_operators_e::GE; }
            | LESS     { $$ = binary_operators_e::LT; }
            | GREATER  { $$ = binary_operators_e::GT; }
;

bin_oper_pls: ADD { $$ = binary_operators_e::ADD; }
            | SUB { $$ = binary_operators_e::SUB; }
;

bin_oper_mul: MUL { $$ = binary_operators_e::MUL; }
            | DIV { $$ = binary_operators_e::DIV; }
            | MOD { $$ = binary_operators_e::MOD; }
;

un_oper: ADD  { $$ = unary_operators_e::ADD; }
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
}
}