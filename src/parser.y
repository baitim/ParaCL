/*
Grammar:
    program      -> statements
    statements   -> statements statement | statements; | statements scope | empty
    scope        -> '{' statements '}'
    statement    -> fork  | loop | instruction
    instruction  -> expression;
    rvalue       -> expression | array_repeat
    expression   -> print | assignment | expression_lgc

    fork         -> if condition body | if condition body else body
    loop         -> while condition body

    condition    -> '(' expression ')'
    body         -> scope | lghost_scope statement rghost_scope | ;

    print        -> print expression
    assignment   -> variable indexes = rvalue

    expression_lgc -> expression_lgc bin_oper_lgc expression_cmp | expression_cmp
    expression_cmp -> expression_cmp bin_oper_cmp expression_pls | expression_pls
    expression_pls -> expression_pls bin_oper_pls expression_mul | expression_mul
    expression_mul -> expression_mul bin_oper_mul terminal       | terminal
    terminal       -> '(' expression ')' | number | undef | array | ? | un_oper terminal | variable indexes
    variable       -> id

    array          -> array '(' array_values ')' indexes
    array_repeat   -> repeat_values indexes
    repeat_values  -> repeat '(' expression, expression ')'
    list_values    -> array_values, array_value | array_value
    array_value    -> expression | repeat_values
    indexes        -> indexes index | empty
    index          -> '[' expression ']'
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
    namespace yy { class driver_t; }
}

%param       { yy::driver_t* driver }
%parse-param { node::node_scope_t*& root }

%code
{
    #include "driver.hpp"
    namespace yy {
        parser::token_type yylex(parser::semantic_type* yylval,
                                 location* loc,
                                 driver_t* driver);
    }
}

%token
    PRINT
    INPUT
    IF
    ELSE
    LOOP

    UNDEF
    COMMA
    ARRAY
    REPEAT

    LBRACKET_ROUND
    RBRACKET_ROUND
    LBRACKET_SQUARE
    RBRACKET_SQUARE
    LBRACKET_CURLY
    RBRACKET_CURLY

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
%nterm <node_expression_t*> rvalue
%nterm <node_expression_t*> expression

%nterm <node_statement_t*>  fork
%nterm <node_statement_t*>  loop
%nterm <node_expression_t*> condition

%nterm <node_expression_t*> print
%nterm <node_expression_t*> assignment

%nterm <node_expression_t*> terminal
%nterm <std::string>        variable

%nterm <node_array_t*>         array
%nterm <node_array_t*>         array_repeat
%nterm <node_repeat_values_t*> repeat_values
%nterm <node_list_values_t*>   list_values
%nterm <node_array_value_t*>   array_value
%nterm <node_indexes_t*>       indexes
%nterm <node_expression_t*>    index

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

    node_variable_t* decl_var(std::string_view name, const yy::location& loc, yy::driver_t* driver) {
        node_variable_t* var = current_scope->get_node(name);
        if (!var) {
            var = driver->add_node<node_variable_t>(loc, name.length(), name);
            current_scope->add_variable(var);
        }
        return var;
    }
}

%start program

%%

program: statements { root = $1; }
;

statements: %empty                 {
                                        $$ = driver->add_node<node_scope_t>(@$, 1, current_scope);
                                        drill_down_to_scope($$);
                                   }
          | statements statement   { $$ = $1; $$->add_statement($2); }
          | statements SCOLON      { $$ = $1; }
          | statements scope       { $$ = $1; $$->add_statement($2); lift_up_from_scope(); }
;

scope: LBRACKET_CURLY statements RBRACKET_CURLY { $$ = $2; }

statement: fork         { $$ = $1; }
         | loop         { $$ = $1; }
         | instruction  { $$ = $1; }
;

instruction: expression SCOLON { $$ = driver->add_node<node_instruction_t>(@$, 1, $1); }
;

rvalue: expression   { $$ = $1; }
      | array_repeat { $$ = $1; }
;

expression: print          { $$ = $1; }
          | assignment     { $$ = $1; }
          | expression_lgc { $$ = $1; }
;

fork: IF condition body %prec THEN  { 
                                        $$ = driver->add_node<node_fork_t>(@1, 2,
                                                $2, $3, driver->add_node<node_scope_t>(@3, 1, current_scope));
                                    }
    | IF condition body ELSE body   { $$ = driver->add_node<node_fork_t>(@1, 2, $2, $3, $5); }
;

loop: LOOP condition body { $$ = driver->add_node<node_loop_t>(@1, 5, $2, $3); }
;

condition: LBRACKET_ROUND expression RBRACKET_ROUND { $$ = $2; }
;

body: scope                                 { $$ = $1; lift_up_from_scope(); }
    | lghost_scope statement rghost_scope   { $1->add_statement($2); $$ = $1; }
    | SCOLON                                { $$ = driver->add_node<node_scope_t>(@$, 1, current_scope); }
;

lghost_scope: %empty { $$ = driver->add_node<node_scope_t>(@$, 1, current_scope); drill_down_to_scope($$); }
rghost_scope: %empty { lift_up_from_scope(); }

print: PRINT expression { $$ = driver->add_node<node_print_t>(@1, 5, $2); }
;

assignment: variable indexes ASSIGN rvalue
        {
            node_variable_t* var    = decl_var($1, @1, driver);
            node_lvalue_t*   lvalue = driver->add_node<node_lvalue_t>(@1, $1.length(), var, $2);
            $$ = driver->add_node<node_assign_t>(@3, 1, lvalue, $4);
        }
;

expression_lgc: expression_lgc bin_oper_lgc expression_cmp { $$ = driver->add_node<node_bin_op_t>(@2, 1, $2, $1, $3); }
              | expression_cmp                             { $$ = $1; }
;

expression_cmp: expression_cmp bin_oper_cmp expression_pls { $$ = driver->add_node<node_bin_op_t>(@2, 1, $2, $1, $3); }
              | expression_pls                             { $$ = $1; }
;

expression_pls: expression_pls bin_oper_pls expression_mul { $$ = driver->add_node<node_bin_op_t>(@2, 1, $2, $1, $3); }
              | expression_mul                             { $$ = $1; }
;

expression_mul: expression_mul bin_oper_mul terminal       { $$ = driver->add_node<node_bin_op_t>(@2, 1, $2, $1, $3); }
              | terminal                                   { $$ = $1; }
;

terminal: LBRACKET_ROUND expression RBRACKET_ROUND { $$ = $2; }
        | NUMBER            { $$ = driver->add_node<node_number_t>(@1, std::to_string($1).length(), $1); }
        | UNDEF             { $$ = driver->add_node<node_undef_t>(@1, 5); }
        | INPUT             { $$ = driver->add_node<node_input_t>(@1, 5); }
        | array             { $$ = $1; }
        | un_oper terminal  { $$ = driver->add_node<node_un_op_t>(@1, 1, $1, $2); }
        | variable indexes  { $$ = driver->add_node<node_lvalue_t>(@1, $1.length(),
                                                                   current_scope->get_node($1), $2); }
;

variable: ID { $$ = $1; }
;

array: ARRAY LBRACKET_ROUND list_values RBRACKET_ROUND indexes
        {
            $$ = driver->add_node<node_array_t>(@1, 5, $3, $5);
            current_scope->add_array($$);
        }
;

array_repeat: repeat_values indexes
        {
            $$ = driver->add_node<node_array_t>(@1, 6, $1, $2);
            current_scope->add_array($$);
        }
;

repeat_values: REPEAT LBRACKET_ROUND expression COMMA expression RBRACKET_ROUND
        { $$ = driver->add_node<node_repeat_values_t>(@1, 6, $3, $5); }
;

list_values: list_values COMMA array_value { $$ = $1; $$->add_value($3); }
           | array_value { $$ = driver->add_node<node_list_values_t>(@$, 1); $$->add_value($1); }
;

array_value: expression    { $$ = driver->add_node<node_expression_value_t>(@$, 1, $1); }
           | repeat_values { $$ = $1; }
;

indexes: %empty        { $$ = driver->add_node<node_indexes_t>(@$, 1); }
       | indexes index { $$ = $1; $$->add_index($2); }
;

index: LBRACKET_SQUARE expression RBRACKET_SQUARE { $$ = $2; }
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
                         driver_t* driver) {
    return driver->yylex(yylval, loc);
}

void parser::error(const location_type& loc, const std::string& message) {
    driver->report_syntax_error(loc);
}
}