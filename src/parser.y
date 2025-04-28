/*
Grammar:
    program      -> global_scope
    global_scope -> global_scope statement    | global_scope scope | global_scope; | empty
    statements   -> statements   statement    | statements   scope | statements;   | statements   return | empty
    statements_r -> statements_r statement_nr | statements_r scope | statements_r;
                  | statements_r expression_s | statements_r return | empty
    
    return       -> return expression;
    scope        -> { statements }
    scope_r      -> { statements_r }

    function      -> function_decl function_body
    function_decl -> foo ( function_args ) function_name
    function_name -> COLON variable | empty
    function_call -> variable_shifted ( function_call_args )
    function_body -> scope_r

    function_args      -> function_args,      variable   | variable   | empty
    function_call_args -> function_call_args, expression | expression | empty

    statement_nr -> fork | loop
    statement    -> statement_nr | expression_s
    expression   -> print | assignment | expression_lgc
    expression_s -> print; | assignment_nr; | expression_lgc; | function | assignment_r

    fork         -> if condition body | if condition body else body
    loop         -> while condition body

    condition    -> ( expression )
    body         -> scope | lghost_scope statement rghost_scope | ;

    print        -> print expression

    rvalue_r      -> function   | scope_r
    rvalue_nr     -> expression | array_repeat
    rvalue_single -> rvalue_r   | expression
    assignment    -> assignment_r | assignment_nr
    assignment_r  -> variable indexes = rvalue_r
    assignment_nr -> variable indexes = rvalue_nr

    expression_lgc -> expression_lgc bin_oper_lgc expression_cmp | expression_cmp
    expression_cmp -> expression_cmp bin_oper_cmp expression_pls | expression_pls
    expression_pls -> expression_pls bin_oper_pls expression_mul | expression_mul
    expression_mul -> expression_mul bin_oper_mul terminal       | terminal
    terminal       -> ( expression ) | number | undef | array | ? |
                                       un_oper terminal | variable_shifted | function_call
    variable         -> id
    variable_shifted -> variable indexes

    array          -> array ( array_values ) indexes
    array_repeat   -> repeat_values indexes
    repeat_values  -> repeat ( rvalue_single, expression )
    list_values    -> array_values, array_value | array_value
    array_value    -> rvalue_single | repeat_values
    indexes        -> indexes index | empty
    index          -> [ expression ]
*/

%language "c++"
%skeleton "lalr1.cc"
%defines
%define api.value.type variant
%locations
%define api.location.file "location.hh"

%code requires
{
    #include "ParaCL/node.hpp"
    using namespace paracl;
    #include <stack>
    namespace yy { class driver_t; }
}

%param       { yy::driver_t* driver }
%parse-param { node_scope_t*& root }
%parse-param { std::string_view program_str }

%code
{
    #include "ParaCL/driver.hpp"
    namespace yy {
        parser::token_type yylex(parser::semantic_type* yylval,
                                 location* loc,
                                 driver_t* driver);
    }
}

%token
    FUNC
    COLON
    RETURN
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

%nterm <node_scope_t*>      global_scope
%nterm <node_scope_t*>      statements
%nterm <node_scope_return_t*> statements_r
%nterm <node_expression_t*> return
%nterm <node_scope_t*>      scope
%nterm <node_scope_return_t*> scope_r

%nterm <node_function_t*>      function
%nterm <node_function_t*>      function_decl
%nterm <node_function_call_wrapper_t*> function_call
%nterm <node_scope_return_t*>  function_body
%nterm <std::pair<std::string, location_t>> function_name

%nterm <node_function_args_t*>      function_args
%nterm <node_function_args_t*>      function_args_empty
%nterm <node_function_args_t*>      function_args_filled
%nterm <node_function_call_args_t*> function_call_args
%nterm <node_function_call_args_t*> function_call_args_empty
%nterm <node_function_call_args_t*> function_call_args_filled

%nterm <node_scope_t*>      body
%nterm <node_scope_t*>      lghost_scope
%nterm                      rghost_scope

%nterm <node_statement_t*>  statement
%nterm <node_statement_t*>  statement_nr
%nterm <node_expression_t*> expression
%nterm <node_expression_t*> expression_s

%nterm <node_statement_t*>  fork
%nterm <node_statement_t*>  loop
%nterm <node_expression_t*> condition

%nterm <node_expression_t*> print

%nterm <node_expression_t*> rvalue_r
%nterm <node_expression_t*> rvalue_nr
%nterm <node_expression_t*> rvalue_single
%nterm <node_expression_t*> assignment
%nterm <node_expression_t*> assignment_r
%nterm <node_expression_t*> assignment_nr

%nterm <node_expression_t*> terminal
%nterm <std::string>        variable
%nterm <node_lvalue_t*>     variable_shifted

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
    std::vector<node_variable_t*> func_args;

    name_table_t global_scope_names;

    std::stack<scope_base_t*> scopes_stack;
    scope_base_t* current_scope = nullptr;

    /* ----------------------------------------------- */

    void drill_down_to_scope(scope_base_t* scope) {
        scopes_stack.push(scope);
        current_scope = scope;
    }

    void lift_up_from_scope() {
        scopes_stack.pop();
        current_scope = scopes_stack.top();
    }

    node_variable_t* decl_var(std::string_view name, std::string_view program_str,
                              const yy::location& loc, yy::driver_t* driver) {
        
        if (global_scope_names.get_var_node(name))
            throw error_declaration_t{make_loc(loc, name.length()), program_str,
                "this name already declared in global scope and can only be used to call function"};

        node_variable_t* var = static_cast<node_variable_t*>(current_scope->get_node(name));
        if (!var) {
            var = driver->add_node<node_variable_t>(loc, name.length(), name);
            current_scope->add_variable(var);
        }
        return var;
    }
}

%start program

%%

program: global_scope { root = $1; }
;

global_scope: %empty                    { $$ = driver->add_node<node_scope_t>(@$, 1, current_scope); drill_down_to_scope($$); }
            | global_scope statement    { $$ = $1; $$->push_statement($2); }
            | global_scope scope        { $$ = $1; $$->push_statement($2); }
            | global_scope SCOLON       { $$ = $1; }
;

statements: %empty                 { $$ = driver->add_node<node_scope_t>(@$, 1, current_scope); drill_down_to_scope($$); }
          | statements statement   { $$ = $1; $$->push_statement($2); }
          | statements scope       { $$ = $1; $$->push_statement($2); }
          | statements SCOLON      { $$ = $1; }
          | statements return      { $$ = $1; $$->set_return($2); }
;

statements_r: %empty                    {
                                            $$ = driver->add_node<node_scope_return_t>(@$, 1, current_scope);
                                            drill_down_to_scope($$);
                                            $$->add_variables(func_args.begin(), func_args.end());
                                        }
          | statements_r statement_nr   { $$ = $1; $$->push_statement_build($2, driver->buf()); }
          | statements_r scope          { $$ = $1; $$->push_statement_build($2, driver->buf()); }
          | statements_r SCOLON         { $$ = $1; }
          | statements_r expression_s   { $$ = $1; $$->push_expression($2, driver->buf()); }
          | statements_r return         { $$ = $1; $$->add_return($2, driver->buf()); }
;

return: RETURN expression_s { $$ = $2; }
;

scope: LBRACKET_CURLY statements RBRACKET_CURLY { $$ = $2; lift_up_from_scope(); }
;

scope_r: LBRACKET_CURLY statements_r RBRACKET_CURLY { $$ = $2; $$->finish_return(); lift_up_from_scope(); }
;

function: function_decl function_body
            {
                $$ = $1;
                $$->bind_body($2);
                func_args = {};
            }
;

function_decl: FUNC LBRACKET_ROUND function_args RBRACKET_ROUND function_name
            {
                std::string_view function_name = $5.first;
                location_t       function_loc  = $5.second;
                if (function_loc.len && global_scope_names.get_var_node(function_name)) {
                    throw error_declaration_t{function_loc, program_str,
                                              "this name already declared in global scope"};}

                $$ = driver->add_node<node_function_t>(@1, 4, $3, nullptr, function_name);
                global_scope_names.add_variable($$);
            }
;

function_name: %empty          {}
             | COLON variable  { $$ = {$2, make_loc(@2, $2.length())}; }
;

function_call: variable indexes LBRACKET_ROUND function_call_args RBRACKET_ROUND
        {
            node_function_t* node_function =
                          static_cast<node_function_t*>(global_scope_names.get_var_node($1));

            if (node_function) {
                if (!$2->empty())
                    throw error_analyze_t{$2->loc(), program_str, "can't index by function"};

                $$ = driver->add_node<node_function_call_wrapper_t>(@1, $1.length(), node_function, $4, true);
            } else {
                node_variable_t* var    = static_cast<node_variable_t*>(current_scope->get_node($1));
                node_lvalue_t*   lvalue = driver->add_node<node_lvalue_t>(@1, $1.length(), var, $2);
                $$ = driver->add_node<node_function_call_wrapper_t>(@1, $1.length(), lvalue, $4, false);
            }
        }
;

function_body: scope_r { $$ = $1; }
;

function_args: function_args_empty  { $$ = $1; }
             | function_args_filled { $$ = $1; }

function_args_empty: %empty { $$ = driver->add_node<node_function_args_t>(@$, 1); }
;

function_args_filled: function_args COMMA variable
                        {
                            $$ = $1;
                            node_variable_t* var = driver->add_node<node_variable_t>(@3, $3.length(), $3);
                            $$->add_arg(var);
                            func_args.push_back(var);
                        }
                    | variable
                        {
                            $$ = driver->add_node<node_function_args_t>(@$, 1);
                            node_variable_t* var = driver->add_node<node_variable_t>(@1, $1.length(), $1);
                            $$->add_arg(var);
                            func_args.push_back(var);
                        }
;

function_call_args: function_call_args_empty  { $$ = $1; }
                  | function_call_args_filled { $$ = $1; }
;

function_call_args_empty: %empty { $$ = driver->add_node<node_function_call_args_t>(@$, 1); }
;

function_call_args_filled:
                    function_call_args COMMA expression { $$ = $1; $$->add_arg($3); }
                  | expression  {
                                    $$ = driver->add_node<node_function_call_args_t>(@$, 1);
                                    $$->add_arg($1);
                                }
;

statement_nr: fork  { $$ = $1; }
            | loop  { $$ = $1; }
;

statement: statement_nr  { $$ = $1; }
         | expression_s  { $$ = driver->add_node<node_instruction_t>(@1, $1->loc().len, $1); }
;

expression: print          { $$ = $1; }
          | assignment     { $$ = $1; }
          | expression_lgc { $$ = $1; }
;

expression_s: print          SCOLON { $$ = $1; }
            | assignment_nr  SCOLON { $$ = $1; }
            | expression_lgc SCOLON { $$ = $1; }
            | function              { $$ = $1; }
            | assignment_r          { $$ = $1; }
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

body: scope   { $$ = $1; }
    | lghost_scope return    rghost_scope             { $1->set_return($2);     $$ = $1; }
    | lghost_scope statement rghost_scope %prec THEN  { $1->push_statement($2); $$ = $1; }
    | SCOLON  { $$ = driver->add_node<node_scope_t>(@$, 1, current_scope); }
;

lghost_scope: %empty { $$ = driver->add_node<node_scope_t>(@$, 1, current_scope); drill_down_to_scope($$); }
rghost_scope: %empty { lift_up_from_scope(); }

print: PRINT expression { $$ = driver->add_node<node_print_t>(@1, 5, $2); }
;

rvalue_r: function  { $$ = $1; }
        | scope_r   { $$ = $1; }
;

rvalue_nr: expression    { $$ = $1; }
         | array_repeat  { $$ = $1; }
;

rvalue_single: rvalue_r    { $$ = $1; }
             | expression  { $$ = $1; }
;

assignment: assignment_r   { $$ = $1; }
          | assignment_nr  { $$ = $1; }
;

assignment_r: variable indexes ASSIGN rvalue_r
        {
            node_variable_t* var    = decl_var($1, program_str, @1, driver);
            node_lvalue_t*   lvalue = driver->add_node<node_lvalue_t>(@1, $1.length(), var, $2);
            $$ = driver->add_node<node_assign_t>(@3, 1, lvalue, $4);
        }
;

assignment_nr: variable indexes ASSIGN rvalue_nr
        {
            node_variable_t* var    = decl_var($1, program_str, @1, driver);
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
        | INPUT             { $$ = driver->add_node<node_input_t>(@1, 1); }
        | array             { $$ = $1; }
        | un_oper terminal  { $$ = driver->add_node<node_un_op_t>(@1, 1, $1, $2); }
        | variable_shifted  { $$ = $1; }
        | function_call     { $$ = $1; }
;

variable: ID { $$ = $1; }
;

variable_shifted: variable indexes
        {
            node_variable_t* var = static_cast<node_variable_t*>(current_scope->get_node($1));
            $$ = driver->add_node<node_lvalue_t>(@1, $1.length(), var, $2);
        }
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

repeat_values: REPEAT LBRACKET_ROUND rvalue_single COMMA expression RBRACKET_ROUND
        { $$ = driver->add_node<node_repeat_values_t>(@1, 6, $3, $5); }
;

list_values: list_values COMMA array_value { $$ = $1; $$->add_value($3); }
           | array_value { $$ = driver->add_node<node_list_values_t>(@$, $1->loc().len); $$->add_value($1); }
;

array_value: rvalue_single  { $$ = driver->add_node<node_expression_value_t>(@$, $1->loc().len, $1); }
           | repeat_values  { $$ = $1; }
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