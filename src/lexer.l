%option c++

%{
#include "parser.tab.hh"
%}

WS       [ \t\n\v]+
NUMBER   [1-9][0-9]*
ID       [a-zA-Z_][a-zA-Z0-9_]*
SCOLON   [;]+

%%

{WS}            // skip
"print"         {/* std::cout << " print <" << yytext << ">\n"; */ return yy::parser::token_type::PRINT;  }

{NUMBER}        {/* std::cout << "number <" << yytext << ">\n"; */ return yy::parser::token_type::NUMBER; }
{ID}            {/* std::cout << "    id <" << yytext << ">\n"; */ return yy::parser::token_type::ID;     }

"=="            {/* std::cout << "     = <" << yytext << ">\n"; */ return yy::parser::token_type::EQUAL;    }
"!="            {/* std::cout << "     = <" << yytext << ">\n"; */ return yy::parser::token_type::NEQUAL;   }
"<="            {/* std::cout << "     + <" << yytext << ">\n"; */ return yy::parser::token_type::ELESS;    }
">="            {/* std::cout << "     - <" << yytext << ">\n"; */ return yy::parser::token_type::EGREATER; }
"<"             {/* std::cout << "     + <" << yytext << ">\n"; */ return yy::parser::token_type::LESS;     }
">"             {/* std::cout << "     - <" << yytext << ">\n"; */ return yy::parser::token_type::GREATER;  }

"="             {/* std::cout << "     = <" << yytext << ">\n"; */ return yy::parser::token_type::ASSIGN; }
"+"             {/* std::cout << "     + <" << yytext << ">\n"; */ return yy::parser::token_type::ADD;    }
"-"             {/* std::cout << "     - <" << yytext << ">\n"; */ return yy::parser::token_type::SUB;    }
"*"             {/* std::cout << "     + <" << yytext << ">\n"; */ return yy::parser::token_type::MUL;    }
"/"             {/* std::cout << "     - <" << yytext << ">\n"; */ return yy::parser::token_type::DIV;    }

{SCOLON}        {/* std::cout << "     ; <" << yytext << ">\n"; */ return yy::parser::token_type::SCOLON; }
.               {/* std::cout << "     . <" << yytext << ">\n"; */ return yy::parser::token_type::ERR;    }

%%

// nothing