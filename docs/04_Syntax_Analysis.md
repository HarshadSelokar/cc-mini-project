# 4. Syntax Analysis

## 4.1 Role of the Parser

The parser checks that the token sequence follows the grammar of MiniLang and builds an **Abstract Syntax Tree**. The lexer knows only words; the parser knows sentences. It knows that `let x = 10;` is a declaration and that `let = 10 x;` is not.

## 4.2 Context-Free Grammar

Nested constructs such as parenthesized expressions and blocks inside blocks cannot be described by regular expressions. They need a **context-free grammar** (CFG). The Bison grammar [`parser/parser.y`](../parser/parser.y) is:

```yacc
program        : statement_list ;

statement_list : %empty
               | statement_list statement ;

statement      : "let" IDENT "=" expression ";"
               | IDENT "=" expression ";"
               | "print" "(" expression ")" ";"
               | if_statement
               | "while" "(" expression ")" block
               | block
               | error ";"                      /* error recovery */ ;

if_statement   : "if" "(" expression ")" block
               | "if" "(" expression ")" block "else" block
               | "if" "(" expression ")" block "else" if_statement ;

block          : "{" statement_list "}" ;

expression     : expression "||" expression
               | expression "&&" expression
               | expression "==" expression  |  expression "!=" expression
               | expression "<"  expression  |  expression ">"  expression
               | expression "<=" expression  |  expression ">=" expression
               | expression "+"  expression  |  expression "-"  expression
               | expression "*"  expression  |  expression "/"  expression
               | expression "%"  expression
               | "-" expression %prec UMINUS
               | "!" expression
               | "(" expression ")"
               | NUMBER | "true" | "false" | IDENT ;
```

`statement_list` is **left-recursive**. This is the preferred form for LR parsers, because the parse stack does not grow with the length of the list.

## 4.3 LALR(1) Parsing

Bison builds an **LALR(1)** parser, a bottom-up shift-reduce parser:

* It keeps a **stack** of grammar symbols and states.
* **Shift** pushes the next token.
* **Reduce** replaces the right-hand side of a production on top of the stack with its left-hand side, and runs the semantic action, which here creates an AST node.
* The action table, indexed by (state, lookahead token), decides between shift, reduce, accept and error.

Parsing `10 + 20 * 2` (simplified):

| Stack | Input | Action |
|---|---|---|
| | `10 + 20 * 2` | shift 10 |
| `NUMBER` | `+ 20 * 2` | reduce `expr → NUMBER` |
| `expr` | `+ 20 * 2` | shift + |
| `expr +` | `20 * 2` | shift 20, reduce to `expr` |
| `expr + expr` | `* 2` | **shift** `*` (higher precedence than `+`) |
| `expr + expr *` | `2` | shift 2, reduce to `expr` |
| `expr + expr * expr` | | reduce `expr * expr` → `expr` |
| `expr + expr` | | reduce `expr + expr` → `expr` |

## 4.4 Precedence and Associativity

The expression grammar above is **ambiguous**: `a + b * c` has two parse trees. Bison resolves the resulting shift/reduce conflicts with precedence declarations, where later lines bind tighter:

```yacc
%left       TOK_OR                      /* ||            lowest  */
%left       TOK_AND                     /* &&                    */
%nonassoc   TOK_EQ TOK_NE               /* == !=                 */
%nonassoc   TOK_LT TOK_GT TOK_LE TOK_GE /* < > <= >=             */
%left       TOK_PLUS TOK_MINUS          /* + -                   */
%left       TOK_MUL TOK_DIV TOK_MOD     /* * / %                 */
%precedence TOK_NOT UMINUS              /* ! unary-   highest    */
```

* **Precedence:** with `expr + expr` on the stack and `*` as the lookahead, `*` has higher precedence than `+`, so Bison shifts. That produces `a + (b * c)`.
* **Left associativity:** with `expr - expr` on the stack and `-` as the lookahead, the precedence is equal and `%left` means reduce. That produces `(a - b) - c`.
* **Non-associativity:** `a < b < c` becomes a syntax error.
* **Unary minus:** `%prec UMINUS` gives `-` in `-a * b` the highest precedence, so it parses as `(-a) * b`.

This is equivalent to the stratified grammar in Chapter 2 (`or_expr → and_expr → … → primary`), but it is shorter, and Bison reports **no conflicts** for the grammar.

The resulting trees (from `tests/parser_tests/precedence.ast`):

```text
let r = a + b * c;              let s = (a + b) * c;
VarDecl 'r'                     VarDecl 's'
`-- BinaryExpr '+'              `-- BinaryExpr '*'
    |-- Variable 'a'                |-- BinaryExpr '+'
    `-- BinaryExpr '*'              |   |-- Variable 'a'
        |-- Variable 'b'            |   `-- Variable 'b'
        `-- Variable 'c'            `-- Variable 'c'
```

## 4.5 Semantic Actions Build the AST

Each production has an action that creates an AST node from the values of its children (`$1`, `$3`, …) and the source location (`@1`, `@2`):

```yacc
| expression TOK_PLUS expression   { $$ = binary(@2, BinaryOp::Add, $1, $3); }
| TOK_LET TOK_IDENTIFIER TOK_ASSIGN expression TOK_SEMICOLON
      { $$ = new VariableDeclaration(at(@1), *$2, own($4)); delete $2; }
```

The actions **never generate code**. They only build the tree, which keeps parsing separate from the later phases. During error recovery, `%destructor` declarations free subtrees that Bison discards, so there are no memory leaks.

## 4.6 Syntax Error Reporting

The grammar uses `%define parse.error custom`, so Bison calls our `yyreport_syntax_error()` with the parser state. That function asks Bison for the set of **expected tokens** and turns it into a specific message:

| Situation | Message |
|---|---|
| `let x = 10` then a new line | `expected ';' after '10' (found 'print')`, reported at the end of line 1 where the `;` is missing |
| `let x = 5 + ;` | `expected an expression, but found ';' after '+'` |
| `let x = (3 * (2 + 1);` | `expected ')' but found ';' (unbalanced parentheses)` |
| missing `}` | `missing '}': end of file reached inside a block` |
| `if x > 0 {` | `unexpected identifier 'x'; expected '('` |
| `x <> 3` | `expected an expression, but found '>' after '<'` |
| `else` without `if` | `unexpected 'else'` |
| `let x = 1;;` | `unexpected ';' (empty statements are not allowed)` |

## 4.7 Error Recovery

The production `statement : error ";"` implements **panic-mode recovery**. On a syntax error Bison pops states until it can shift the special `error` token, then discards input tokens until it finds a `;`, and parsing resumes. So

```text
let x = 1 2;
let = 5;
```

reports **both** errors in one run. A closing `}` left behind by recovery is not reported again, which avoids cascading messages. If any syntax error was reported, the driver stops after parsing.
