/* =============================================================================
 *  parser.y  --  Bison grammar of MiniLang (Phase 2: syntax analysis)
 * -----------------------------------------------------------------------------
 *  Bison generates an LALR(1) parser from this grammar.  The semantic actions
 *  do NOT generate code; they only build AST nodes (see include/AST.h).
 *
 *  Operator precedence and associativity (lowest to highest):
 *
 *      ||                 left
 *      &&                 left
 *      ==  !=             non-associative
 *      <  >  <=  >=       non-associative
 *      +  -               left
 *      *  /  %            left
 *      !  unary -         right
 *
 *  so "a + b * c" parses as a + (b * c) and "a - b - c" as (a - b) - c.
 *
 *  Tokens are supplied by yylex() in src/Parser.cpp, which reads the vector
 *  produced by the lexer phase.
 * ===========================================================================*/

%code requires {
#include "AST.h"
#include <vector>
}

%code {
#include "Parser.h"

#include <string>

using namespace minilang;

int yylex(void);
void yyerror(const char* message);

/* Convert a Bison location into the AST's SourceLoc. */
static SourceLoc at(const YYLTYPE& l) { return SourceLoc{l.first_line, l.first_column}; }

/* Take ownership of a raw pointer produced by a grammar rule. */
template <typename T> static std::unique_ptr<T> own(T* p) { return std::unique_ptr<T>(p); }

static BlockStatement* makeBlock(const YYLTYPE& l, std::vector<Statement*>* list) {
    auto* block = new BlockStatement(at(l));
    for (Statement* s : *list) block->statements.emplace_back(s);
    delete list;
    return block;
}

static Expression* binary(const YYLTYPE& opLoc, BinaryOp op, Expression* l, Expression* r) {
    return new BinaryExpression(at(opLoc), op, own(l), own(r));
}
}

%define parse.error custom
%locations

%union {
    int                                 ival;
    std::string*                        sval;
    minilang::Expression*               expr;
    minilang::Statement*                stmt;
    std::vector<minilang::Statement*>*  stmts;
}

/* -------- terminals (the string aliases are used in error messages) ------ */
%token TOK_LET "'let'" TOK_IF "'if'" TOK_ELSE "'else'" TOK_WHILE "'while'"
%token TOK_PRINT "'print'" TOK_TRUE "'true'" TOK_FALSE "'false'"
%token <sval> TOK_IDENTIFIER "identifier"
%token <ival> TOK_NUMBER "number"
%token TOK_PLUS "'+'" TOK_MINUS "'-'" TOK_MUL "'*'" TOK_DIV "'/'" TOK_MOD "'%'"
%token TOK_ASSIGN "'='" TOK_EQ "'=='" TOK_NE "'!='"
%token TOK_LT "'<'" TOK_GT "'>'" TOK_LE "'<='" TOK_GE "'>='"
%token TOK_AND "'&&'" TOK_OR "'||'" TOK_NOT "'!'"
%token TOK_LPAREN "'('" TOK_RPAREN "')'" TOK_LBRACE "'{'" TOK_RBRACE "'}'"
%token TOK_SEMICOLON "';'"

%type <expr>  expression
%type <stmt>  statement if_statement block
%type <stmts> statement_list

/* Free partially built subtrees when error recovery discards them. */
%destructor { delete $$; } <sval> <expr> <stmt>
%destructor { for (Statement* s : *$$) delete s; delete $$; } <stmts>

/* -------- precedence: later lines bind tighter ----------------------------- */
%left     TOK_OR
%left     TOK_AND
%nonassoc TOK_EQ TOK_NE
%nonassoc TOK_LT TOK_GT TOK_LE TOK_GE
%left     TOK_PLUS TOK_MINUS
%left     TOK_MUL TOK_DIV TOK_MOD
%precedence TOK_NOT UMINUS

%start program

%%

program
    : statement_list
        {
            auto program = std::make_unique<Program>();
            for (Statement* s : *$1) program->statements.emplace_back(s);
            delete $1;
            minilang::detail::parserState.result = std::move(program);
        }
    ;

statement_list
    : %empty                    { $$ = new std::vector<Statement*>(); }
    | statement_list statement  { $$ = $1; if ($2) $$->push_back($2); }
    ;

statement
    : TOK_LET TOK_IDENTIFIER TOK_ASSIGN expression TOK_SEMICOLON
        { $$ = new VariableDeclaration(at(@1), *$2, own($4)); delete $2; }
    | TOK_IDENTIFIER TOK_ASSIGN expression TOK_SEMICOLON
        { $$ = new Assignment(at(@1), *$1, own($3)); delete $1; }
    | TOK_PRINT TOK_LPAREN expression TOK_RPAREN TOK_SEMICOLON
        { $$ = new PrintStatement(at(@1), own($3)); }
    | if_statement
        { $$ = $1; }
    | TOK_WHILE TOK_LPAREN expression TOK_RPAREN block
        { $$ = new WhileStatement(at(@1), own($3), own($5)); }
    | block
        { $$ = $1; }
    | error TOK_SEMICOLON
        { $$ = nullptr; yyerrok; }      /* error recovery: skip to next ';' */
    ;

if_statement
    : TOK_IF TOK_LPAREN expression TOK_RPAREN block
        { $$ = new IfStatement(at(@1), own($3), own($5), nullptr); }
    | TOK_IF TOK_LPAREN expression TOK_RPAREN block TOK_ELSE block
        { $$ = new IfStatement(at(@1), own($3), own($5), own($7)); }
    | TOK_IF TOK_LPAREN expression TOK_RPAREN block TOK_ELSE if_statement
        { $$ = new IfStatement(at(@1), own($3), own($5), own($7)); }
    ;

block
    : TOK_LBRACE statement_list TOK_RBRACE
        { $$ = makeBlock(@1, $2); }
    ;

expression
    : expression TOK_OR  expression     { $$ = binary(@2, BinaryOp::Or,  $1, $3); }
    | expression TOK_AND expression     { $$ = binary(@2, BinaryOp::And, $1, $3); }
    | expression TOK_EQ  expression     { $$ = binary(@2, BinaryOp::Eq,  $1, $3); }
    | expression TOK_NE  expression     { $$ = binary(@2, BinaryOp::Ne,  $1, $3); }
    | expression TOK_LT  expression     { $$ = binary(@2, BinaryOp::Lt,  $1, $3); }
    | expression TOK_GT  expression     { $$ = binary(@2, BinaryOp::Gt,  $1, $3); }
    | expression TOK_LE  expression     { $$ = binary(@2, BinaryOp::Le,  $1, $3); }
    | expression TOK_GE  expression     { $$ = binary(@2, BinaryOp::Ge,  $1, $3); }
    | expression TOK_PLUS  expression   { $$ = binary(@2, BinaryOp::Add, $1, $3); }
    | expression TOK_MINUS expression   { $$ = binary(@2, BinaryOp::Sub, $1, $3); }
    | expression TOK_MUL expression     { $$ = binary(@2, BinaryOp::Mul, $1, $3); }
    | expression TOK_DIV expression     { $$ = binary(@2, BinaryOp::Div, $1, $3); }
    | expression TOK_MOD expression     { $$ = binary(@2, BinaryOp::Mod, $1, $3); }
    | TOK_MINUS expression %prec UMINUS { $$ = new UnaryExpression(at(@1), UnaryOp::Neg, own($2)); }
    | TOK_NOT expression                { $$ = new UnaryExpression(at(@1), UnaryOp::Not, own($2)); }
    | TOK_LPAREN expression TOK_RPAREN  { $$ = $2; }
    | TOK_NUMBER                        { $$ = new IntegerLiteral(at(@1), $1); }
    | TOK_TRUE                          { $$ = new BooleanLiteral(at(@1), true); }
    | TOK_FALSE                         { $$ = new BooleanLiteral(at(@1), false); }
    | TOK_IDENTIFIER                    { $$ = new VariableExpression(at(@1), *$1); delete $1; }
    ;

%%

/* -----------------------------------------------------------------------------
 *  Syntax-error reporting.  Bison calls this with the parser context; we turn
 *  the list of expected tokens into a readable message such as
 *      Parser Error [Line 6, Col 11]: expected ';' after '10'
 * ---------------------------------------------------------------------------*/
static std::string symbolText(yysymbol_kind_t kind) {
    std::string name = yysymbol_name(kind);
    if (name.size() >= 2 && name.front() == '"' && name.back() == '"')
        name = name.substr(1, name.size() - 2);
    return name;
}

static int yyreport_syntax_error(const yypcontext_t* ctx) {
    using minilang::detail::parserState;

    yysymbol_kind_t expected[YYNTOKENS];
    int count = yypcontext_expected_tokens(ctx, expected, YYNTOKENS);
    yysymbol_kind_t lookahead = yypcontext_token(ctx);

    const Token* current  = parserState.lookahead();
    const Token* previous = parserState.previous();

    // Describe what was found.
    std::string found;
    if (lookahead == YYSYMBOL_YYEOF) found = "end of file";
    else if (current && (lookahead == YYSYMBOL_TOK_IDENTIFIER || lookahead == YYSYMBOL_TOK_NUMBER))
        found = symbolText(lookahead) + " '" + current->lexeme + "'";
    else found = symbolText(lookahead);

    // Classify the expected set.
    bool wantsSemicolon = false, wantsExpression = false, wantsRParen = false, wantsRBrace = false;
    std::vector<std::string> names;
    for (int i = 0; i < count; ++i) {
        if (expected[i] == YYSYMBOL_TOK_SEMICOLON) wantsSemicolon = true;
        if (expected[i] == YYSYMBOL_TOK_RPAREN) wantsRParen = true;
        if (expected[i] == YYSYMBOL_TOK_RBRACE) wantsRBrace = true;
        if (expected[i] == YYSYMBOL_TOK_NUMBER)   // numbers only appear in expressions
            wantsExpression = true;
    }
    for (int i = 0; i < count; ++i) {
        yysymbol_kind_t k = expected[i];
        bool startsExpr = k == YYSYMBOL_TOK_NUMBER || k == YYSYMBOL_TOK_IDENTIFIER ||
                          k == YYSYMBOL_TOK_TRUE || k == YYSYMBOL_TOK_FALSE ||
                          k == YYSYMBOL_TOK_LPAREN || k == YYSYMBOL_TOK_MINUS ||
                          k == YYSYMBOL_TOK_NOT;
        if (wantsExpression && startsExpr) continue;
        names.push_back(symbolText(k));
    }
    if (wantsExpression) names.insert(names.begin(), "an expression");

    // Error recovery skips to the next ';', which may leave the closing '}' of
    // the broken block behind.  Reporting it would only repeat the first error.
    if (lookahead == YYSYMBOL_TOK_RBRACE && parserState.diag->errorCount() > 0) return 0;

    SourceLoc loc = current ? current->loc : SourceLoc{};
    std::string message;

    if (wantsSemicolon && previous) {
        // A missing ';' belongs to the end of the previous token's line.
        loc = SourceLoc{previous->loc.line,
                        previous->loc.column + static_cast<int>(previous->lexeme.size())};
        message = "expected ';' after '" + previous->lexeme + "' (found " + found + ")";
    } else if (lookahead == YYSYMBOL_TOK_SEMICOLON && previous &&
               (previous->kind == TOK_SEMICOLON || previous->kind == TOK_RBRACE)) {
        message = "unexpected ';' (empty statements are not allowed)";
    } else if (wantsRParen && !wantsExpression) {
        message = "expected ')' but found " + found + " (unbalanced parentheses)";
    } else if (wantsRBrace && lookahead == YYSYMBOL_YYEOF) {
        message = "missing '}': end of file reached inside a block";
    } else if (wantsExpression && !names.empty() && names.size() <= 2) {
        message = "expected " + names[0] + (names.size() == 2 ? " or " + names[1] : "") +
                  ", but found " + found;
        if (previous) message += " after '" + previous->lexeme + "'";
    } else if (!names.empty() && names.size() <= 4) {
        message = "unexpected " + found + "; expected ";
        for (size_t i = 0; i < names.size(); ++i) {
            if (i) message += (i + 1 == names.size()) ? " or " : ", ";
            message += names[i];
        }
    } else {
        message = "unexpected " + found;
    }

    parserState.diag->error(Phase::Parser, loc, message);
    return 0;
}

void yyerror(const char* message) {
    using minilang::detail::parserState;
    const Token* current = parserState.lookahead();
    parserState.diag->error(Phase::Parser, current ? current->loc : SourceLoc{}, message);
}
