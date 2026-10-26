/* =============================================================================
 *  lexer.l  --  Flex specification of the MiniLang lexical analyzer
 * -----------------------------------------------------------------------------
 *  Each rule pairs a regular expression with an action.  Flex combines all
 *  patterns into one DFA; when several patterns match it picks
 *      1. the longest match ("<=" beats "<"), then
 *      2. the rule listed first (keyword "let" beats the identifier rule).
 *
 *  Actions return the token codes declared in parser/parser.y (TOK_LET, ...).
 *  Whitespace and comments produce no tokens.  Characters that do not start
 *  any valid token are reported as lexical errors and skipped.
 *
 *  Position tracking: YY_USER_ACTION runs before every action and records
 *  the line/column where the matched text starts.
 * ===========================================================================*/

%option noyywrap nounistd never-interactive nodefault noinput nounput
%option warn

%{
#include "Lexer.h"
#include "parser.tab.hpp"

#include <cstdio>
#include <string>

using minilang::detail::scanState;

// The parser has its own yylex() (it reads the token vector), so the Flex
// scanner function gets a different name.
#define YY_DECL int minilangScan(void)
int minilangScan(void);

#define YY_USER_ACTION                              \
    scanState.tokenLine = scanState.line;           \
    scanState.tokenColumn = scanState.column;       \
    scanState.column += static_cast<int>(yyleng);

static void lexError(const std::string& message) {
    scanState.diag->error(minilang::Phase::Lexer,
                          minilang::SourceLoc{scanState.tokenLine, scanState.tokenColumn},
                          message);
}

/* Show a character in an error message; non-printable bytes as hex. */
static std::string describeChar(unsigned char c) {
    if (c >= 32 && c < 127) return std::string("'") + static_cast<char>(c) + "'";
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%02X", c);
    return buf;
}
%}

DIGIT       [0-9]
LETTER      [A-Za-z_]
IDENT       {LETTER}({LETTER}|{DIGIT})*

%x BLOCK_COMMENT

%%

 /* ---------------- whitespace and comments (no tokens) ---------------- */
[ \t\r\f\v]+            { /* skip blanks */ }
\n                      { scanState.line++; scanState.column = 1; }
"//"[^\n]*              { /* line comment */ }
"/*"                    { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/"     { BEGIN(INITIAL); }
<BLOCK_COMMENT>\n       { scanState.line++; scanState.column = 1; }
<BLOCK_COMMENT>[^*\n]+  { }
<BLOCK_COMMENT>"*"      { }
<BLOCK_COMMENT><<EOF>>  { lexError("unterminated block comment '/*'"); BEGIN(INITIAL); return 0; }

 /* ---------------- keywords (listed before identifiers) ---------------- */
"let"                   { return TOK_LET; }
"if"                    { return TOK_IF; }
"else"                  { return TOK_ELSE; }
"while"                 { return TOK_WHILE; }
"print"                 { return TOK_PRINT; }
"true"                  { return TOK_TRUE; }
"false"                 { return TOK_FALSE; }

 /* ---------------- identifiers and integer literals -------------------- */
{IDENT}                 { return TOK_IDENTIFIER; }
{DIGIT}+                { return TOK_NUMBER; }
{DIGIT}+{LETTER}({LETTER}|{DIGIT})*  {
                          lexError("invalid numeric literal '" + std::string(yytext) +
                                   "' (identifiers cannot start with a digit)");
                        }

 /* ---------------- operators (two-character ones first) ---------------- */
"=="                    { return TOK_EQ; }
"!="                    { return TOK_NE; }
"<="                    { return TOK_LE; }
">="                    { return TOK_GE; }
"&&"                    { return TOK_AND; }
"||"                    { return TOK_OR; }
"+"                     { return TOK_PLUS; }
"-"                     { return TOK_MINUS; }
"*"                     { return TOK_MUL; }
"/"                     { return TOK_DIV; }
"%"                     { return TOK_MOD; }
"="                     { return TOK_ASSIGN; }
"<"                     { return TOK_LT; }
">"                     { return TOK_GT; }
"!"                     { return TOK_NOT; }

 /* ---------------- delimiters ------------------------------------------ */
"("                     { return TOK_LPAREN; }
")"                     { return TOK_RPAREN; }
"{"                     { return TOK_LBRACE; }
"}"                     { return TOK_RBRACE; }
";"                     { return TOK_SEMICOLON; }

 /* ---------------- lexical errors -------------------------------------- */
\"[^"\n]*\"?           { lexError("string literals are not supported in MiniLang (only int and bool)"); }
"&"                     { lexError("unexpected character '&' (did you mean '&&'?)"); }
"|"                     { lexError("unexpected character '|' (did you mean '||'?)"); }
[\x80-\xFF]+            { lexError("unexpected non-ASCII character sequence"); }
.                       { lexError("unexpected character " +
                                   describeChar(static_cast<unsigned char>(yytext[0]))); }

%%

/* ---------------- interface used by src/Lexer.cpp ------------------------ */
namespace minilang {
namespace detail {

static YY_BUFFER_STATE currentBuffer = nullptr;

void scannerBegin(const std::string& source) {
    currentBuffer = yy_scan_bytes(source.data(), static_cast<int>(source.size()));
    BEGIN(INITIAL);
}

int scannerNext() { return minilangScan(); }

std::string scannerText() { return std::string(yytext, static_cast<size_t>(yyleng)); }

void scannerEnd() {
    if (currentBuffer) yy_delete_buffer(currentBuffer);
    currentBuffer = nullptr;
    yylex_destroy();
}

} // namespace detail
} // namespace minilang
