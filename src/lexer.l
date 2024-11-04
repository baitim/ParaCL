%option c++ yylineno noyywrap
%option yyclass="yy::Lexer_t"

%{
    #include "parser.tab.hh"
    #include "driver.hpp"
    #define YY_USER_ACTION loc.columns(yyleng);
%}

WS       [ \t\v]+
NS       [\n]+
NUMBER   [1-9][0-9]*|0
ID       [a-zA-Z_][a-zA-Z0-9_]*
SCOLON   [;]+

%%
%{
    loc.step(); // start = end value
%}

{WS}            { loc.step(); }
{NS}            { loc.lines(yyleng); loc.step(); }

"print"         {/* std::cout << " print <" << yytext << ">\n"; */ return yy::parser::token_type::PRINT; }
"?"             {/* std::cout << "     ? <" << yytext << ">\n"; */ return yy::parser::token_type::INPUT; }
"if"            {/* std::cout << "    if <" << yytext << ">\n"; */ return yy::parser::token_type::FORK1; }
"else"          {/* std::cout << "  else <" << yytext << ">\n"; */ return yy::parser::token_type::FORK2; }
"while"         {/* std::cout << " while <" << yytext << ">\n"; */ return yy::parser::token_type::LOOP;  }

{NUMBER}        {/* std::cout << "number <" << yytext << ">\n"; */ return yy::parser::token_type::NUMBER; }
{ID}            {/* std::cout << "    id <" << yytext << ">\n"; */ return yy::parser::token_type::ID;     }

"("             {/* std::cout << "     ( <" << yytext << ">\n"; */ return yy::parser::token_type::LBRACKET; }
")"             {/* std::cout << "     ) <" << yytext << ">\n"; */ return yy::parser::token_type::RBRACKET; }
"{"             {/* std::cout << "     { <" << yytext << ">\n"; */ return yy::parser::token_type::LSCOPE;   }
"}"             {/* std::cout << "     } <" << yytext << ">\n"; */ return yy::parser::token_type::RSCOPE;   }

"=="            {/* std::cout << "    == <" << yytext << ">\n"; */ return yy::parser::token_type::EQUAL;    }
"!="            {/* std::cout << "    != <" << yytext << ">\n"; */ return yy::parser::token_type::NEQUAL;   }
"<="            {/* std::cout << "    <= <" << yytext << ">\n"; */ return yy::parser::token_type::ELESS;    }
">="            {/* std::cout << "    >= <" << yytext << ">\n"; */ return yy::parser::token_type::EGREATER; }
"<"             {/* std::cout << "     < <" << yytext << ">\n"; */ return yy::parser::token_type::LESS;     }
">"             {/* std::cout << "     > <" << yytext << ">\n"; */ return yy::parser::token_type::GREATER;  }

"="             {/* std::cout << "     = <" << yytext << ">\n"; */ return yy::parser::token_type::ASSIGN; }
"+"             {/* std::cout << "     + <" << yytext << ">\n"; */ return yy::parser::token_type::ADD;    }
"-"             {/* std::cout << "     - <" << yytext << ">\n"; */ return yy::parser::token_type::SUB;    }
"*"             {/* std::cout << "     * <" << yytext << ">\n"; */ return yy::parser::token_type::MUL;    }
"/"             {/* std::cout << "     / <" << yytext << ">\n"; */ return yy::parser::token_type::DIV;    }

{SCOLON}        {/* std::cout << "     ; <" << yytext << ">\n"; */ return yy::parser::token_type::SCOLON; }
.               { std::cout << print_red(" error token <" << yytext << ">\n"); return yy::parser::token_type::ERR;    }

%%

// nothing