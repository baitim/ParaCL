
#ifndef yyFlexLexer
#include <FlexLexer.h>
#endif

namespace yy {
struct Lexer_t : public yyFlexLexer {
    location loc;
    int yylex() override;
};
}