%option c++ yylineno noyywrap
%option yyclass="yy::lexer_t"

%{
    #include "parser.tab.hh"
    #include "ParaCL/driver.hpp"
    #define YY_USER_ACTION update_new_token();
%}

WS       [ \t\v]+
NS       [\n]+
NUMBER   [1-9][0-9]*|0
ID       [a-zA-Z_][a-zA-Z0-9_]*
SCOLON   [;]+
COMMENTS "//".*

%%

{WS}            // nothing
{COMMENTS}      // nothing
{NS}            { update_new_line(); }

"func"          { return yy::parser::token_type::FUNC;   }
":"             { return yy::parser::token_type::COLON;  }
"return"        { return yy::parser::token_type::RETURN; }
"print"         { return yy::parser::token_type::PRINT;  }
"?"             { return yy::parser::token_type::INPUT;  }
"if"            { return yy::parser::token_type::IF;     }
"else"          { return yy::parser::token_type::ELSE;   }
"while"         { return yy::parser::token_type::LOOP;   }
"undef"         { return yy::parser::token_type::UNDEF;  }
","             { return yy::parser::token_type::COMMA;  }
"array"         { return yy::parser::token_type::ARRAY;  }
"repeat"        { return yy::parser::token_type::REPEAT; }

{NUMBER}        { return yy::parser::token_type::NUMBER; }
{ID}            { return yy::parser::token_type::ID;     }

"("             { return yy::parser::token_type::LBRACKET_ROUND;  }
")"             { return yy::parser::token_type::RBRACKET_ROUND;  }
"["             { return yy::parser::token_type::LBRACKET_SQUARE; }
"]"             { return yy::parser::token_type::RBRACKET_SQUARE; }
"{"             { return yy::parser::token_type::LBRACKET_CURLY;  }
"}"             { return yy::parser::token_type::RBRACKET_CURLY;  }

"=="            { return yy::parser::token_type::EQUAL;    }
"!="            { return yy::parser::token_type::NEQUAL;   }
"<="            { return yy::parser::token_type::ELESS;    }
">="            { return yy::parser::token_type::EGREATER; }
"<"             { return yy::parser::token_type::LESS;     }
">"             { return yy::parser::token_type::GREATER;  }

"||"            { return yy::parser::token_type::OR;   }
"&&"            { return yy::parser::token_type::AND;  }
"!"             { return yy::parser::token_type::NOT;  }

"="             { return yy::parser::token_type::ASSIGN; }
"+"             { return yy::parser::token_type::ADD;    }
"-"             { return yy::parser::token_type::SUB;    }
"*"             { return yy::parser::token_type::MUL;    }
"/"             { return yy::parser::token_type::DIV;    }
"%"             { return yy::parser::token_type::MOD;    }

{SCOLON}        { return yy::parser::token_type::SCOLON; }

.               { return yy::parser::token_type::ERR; }

%%

// nothing