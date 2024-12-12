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
    array_repeat   -> repeat indexes
    repeat_values  -> repeat '(' expression, expression ')'
    list_values    -> array_values, list_value | list_value
    list_value     -> expression | repeat_values
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
    namespace yy {class driver_t;}
}

%param       { yy::driver_t* driver }
%parse-param { node::buffer_t& buf }
%parse-param { node::node_scope_t*& root }
%parse-param { environments::environments_t& parse_env }

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
%nterm <node_array_value_t*>    list_value
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

    node_variable_t* decl_var(std::string_view name, buffer_t& buf) {
        node_variable_t* var = current_scope->get_node(name);
        if (!var) {
            var = buf.add_node<node_variable_t>(name);
            current_scope->add_variable(var);
        }
        return var;
    }

    node_variable_t* get_var(std::string_view name, const yy::location& loc, const yy::driver_t* driver) {
        node_variable_t* var = current_scope->get_node(name);
        if (!var)
            driver->report_undecl_error(loc, name);
        return var;
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

scope: LBRACKET_CURLY statements RBRACKET_CURLY { $$ = $2; }

statement: fork         { $$ = $1; }
         | loop         { $$ = $1; }
         | instruction  { $$ = $1; }
;

instruction: expression SCOLON { $$ = buf.add_node<node_instruction_t>($1); }
;

rvalue: expression   { $$ = $1; }
      | array_repeat { $$ = $1; }
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

condition: LBRACKET_ROUND expression RBRACKET_ROUND { $$ = $2; }
;

body: scope                                 { $$ = $1; lift_up_from_scope(); }
    | lghost_scope statement rghost_scope   { $1->add_statement($2); $$ = $1; }
    | SCOLON                                { $$ = buf.add_node<node_scope_t>(current_scope); }
;

lghost_scope: %empty { $$ = buf.add_node<node_scope_t>(current_scope); drill_down_to_scope($$); }
rghost_scope: %empty { lift_up_from_scope(); }

print: PRINT expression { $$ = buf.add_node<node_print_t>($2); }
;

assignment: variable indexes ASSIGN rvalue
        {
            node_variable_t* var    = decl_var($1, buf);
            node_lvalue_t*   lvalue = buf.add_node<node_lvalue_t>(var, $2);
            $$ = buf.add_node<node_assign_t>(lvalue, $4);
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

terminal: LBRACKET_ROUND expression RBRACKET_ROUND { $$ = $2; }
        | NUMBER            { $$ = buf.add_node<node_number_t>($1); }
        | UNDEF             { $$ = buf.add_node<node_undef_t>(); }
        | INPUT             { $$ = buf.add_node<node_input_t>(); }
        | array             { $$ = $1; }
        | un_oper terminal  { $$ = buf.add_node<node_un_op_t>($1, $2); }
        | variable indexes  {
                                node_variable_t* var = get_var($1, @1, driver);
                                $$ = buf.add_node<node_lvalue_t>(var, $2);
                            }
;

variable: ID { $$ = $1; }
;

array: ARRAY LBRACKET_ROUND list_values RBRACKET_ROUND indexes { $$ = buf.add_node<node_array_t>($3, $5); }
;

array_repeat: repeat_values indexes { $$ = buf.add_node<node_array_t>($1, $2); }
;

repeat_values: REPEAT LBRACKET_ROUND expression COMMA expression RBRACKET_ROUND
               { $$ = buf.add_node<node_repeat_values_t>($3, $5); }
;

list_values: list_values COMMA list_value { $$ = $1; $$->add_value($3); }
           | list_value { $$ = buf.add_node<node_list_values_t>(); $$->add_value($1); }
;

list_value: expression    { $$ = buf.add_node<node_expression_value_t>($1); }
          | repeat_values { $$ = $1; }
;

indexes: %empty        { $$ = buf.add_node<node_indexes_t>(); }
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