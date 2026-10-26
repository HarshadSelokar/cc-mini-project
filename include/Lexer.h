// =============================================================================
//  Lexer.h  --  Phase 1: Lexical analysis
// -----------------------------------------------------------------------------
//  The scanning rules live in parser/lexer.l and are turned into C code by
//  Flex.  This class wraps the generated scanner:
//
//      Lexer lexer(source, diagnostics);
//      std::vector<Token> tokens = lexer.tokenize();
//
//  tokenize() returns the complete token stream ending with an EOF token.
//  Invalid characters and out-of-range integer literals are reported as
//  "Lexer Error" diagnostics; scanning continues so that several errors can
//  be reported in one run.
// =============================================================================
#pragma once

#include "Diagnostics.h"
#include "Token.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace minilang {

class Lexer {
public:
    Lexer(const std::string& source, Diagnostics& diagnostics);

    std::vector<Token> tokenize();

    // Pretty-print a token stream: a table followed by a compact listing.
    static void printTokens(const std::vector<Token>& tokens, std::ostream& out);

private:
    const std::string& source_;
    Diagnostics& diag_;
};

namespace detail {
// State shared with the Flex-generated scanner (see parser/lexer.l).
struct ScanState {
    int line = 1;          // current position in the input
    int column = 1;
    int tokenLine = 1;     // start position of the token just matched
    int tokenColumn = 1;
    Diagnostics* diag = nullptr;
};
extern ScanState scanState;

// Implemented in the user-code section of parser/lexer.l.
void scannerBegin(const std::string& source);
int scannerNext();                 // returns a token code, 0 at end of input
std::string scannerText();         // lexeme of the token just returned
void scannerEnd();
} // namespace detail

} // namespace minilang
