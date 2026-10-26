// =============================================================================
//  Token.h  --  Output of the lexical-analysis phase
// -----------------------------------------------------------------------------
//  The lexer turns the character stream into a vector<Token>.  The token kind
//  is the integer code that Bison assigns to each terminal symbol (TOK_LET,
//  TOK_IDENTIFIER, ... declared in parser/parser.y), so the parser can consume
//  the tokens directly without any translation table.
// =============================================================================
#pragma once

#include "Diagnostics.h"

#include <string>

namespace minilang {

struct Token {
    int kind = 0;            // Bison token code; 0 means end of file
    std::string lexeme;      // exact text matched in the source
    SourceLoc loc;           // where the token starts
};

// Human-readable token class name, e.g. "LET", "IDENTIFIER", "PLUS", "EOF".
std::string tokenName(int kind);

} // namespace minilang
