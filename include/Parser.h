// =============================================================================
//  Parser.h  --  Phase 2: Syntax analysis
// -----------------------------------------------------------------------------
//  The grammar lives in parser/parser.y and is compiled by Bison into an
//  LALR(1) parser.  This class feeds it the token vector produced by the
//  lexer and returns the AST:
//
//      Parser parser(tokens, diagnostics);
//      std::unique_ptr<Program> ast = parser.parse();   // null on error
// =============================================================================
#pragma once

#include "AST.h"
#include "Diagnostics.h"
#include "Token.h"

#include <memory>
#include <vector>

namespace minilang {

class Parser {
public:
    Parser(const std::vector<Token>& tokens, Diagnostics& diagnostics);

    // Returns the AST, or nullptr if any syntax error was reported.
    std::unique_ptr<Program> parse();

private:
    const std::vector<Token>& tokens_;
    Diagnostics& diag_;
};

namespace detail {
// State shared with the Bison-generated parser (yylex / error reporting).
struct ParserState {
    const std::vector<Token>* tokens = nullptr;
    size_t next = 0;                         // index of the next token to hand out
    Diagnostics* diag = nullptr;
    std::unique_ptr<Program> result;

    // Token most recently returned by yylex (Bison's lookahead).
    const Token* lookahead() const {
        return (tokens && next > 0) ? &(*tokens)[next - 1] : nullptr;
    }
    // Token before the lookahead (the last token that was accepted).
    const Token* previous() const {
        return (tokens && next > 1) ? &(*tokens)[next - 2] : nullptr;
    }
};
extern ParserState parserState;
} // namespace detail

} // namespace minilang
