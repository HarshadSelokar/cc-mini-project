// =============================================================================
//  Parser.cpp  --  Phase 2: Syntax analysis (driver around the Bison parser)
// -----------------------------------------------------------------------------
//  Bison's generated yyparse() pulls tokens by calling yylex().  Instead of
//  running Flex a second time, yylex() here simply hands out the tokens the
//  lexer phase already produced, filling in the semantic value (yylval) and
//  source location (yylloc) that the grammar actions use.
// =============================================================================
#include "Parser.h"
#include "parser.tab.hpp"

#include <cstdlib>

namespace minilang {

namespace detail {
ParserState parserState;
}

Parser::Parser(const std::vector<Token>& tokens, Diagnostics& diagnostics)
    : tokens_(tokens), diag_(diagnostics) {}

std::unique_ptr<Program> Parser::parse() {
    using detail::parserState;
    parserState = detail::ParserState{};
    parserState.tokens = &tokens_;
    parserState.diag = &diag_;

    int errorsBefore = diag_.errorCount();
    int status = yyparse();

    std::unique_ptr<Program> program = std::move(parserState.result);
    parserState = detail::ParserState{};

    if (status != 0 || diag_.errorCount() != errorsBefore) return nullptr;
    return program;
}

} // namespace minilang

// Called by yyparse() for every token.
int yylex(void) {
    using minilang::detail::parserState;
    const auto& tokens = *parserState.tokens;
    // The lexer always appends an EOF token; keep returning it at the end.
    const minilang::Token& tok =
        tokens[parserState.next < tokens.size() ? parserState.next++ : tokens.size() - 1];

    yylloc.first_line = yylloc.last_line = tok.loc.line;
    yylloc.first_column = tok.loc.column;
    yylloc.last_column = tok.loc.column + static_cast<int>(tok.lexeme.size());

    if (tok.kind == TOK_IDENTIFIER) {
        yylval.sval = new std::string(tok.lexeme);   // freed by the grammar action
    } else if (tok.kind == TOK_NUMBER) {
        yylval.ival = static_cast<int>(std::strtol(tok.lexeme.c_str(), nullptr, 10));
    }
    return tok.kind;
}
