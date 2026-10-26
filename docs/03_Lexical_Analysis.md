# 3. Lexical Analysis

## 3.1 Role of the Lexer

The lexer (scanner) is the first phase. It reads the source as a stream of characters and groups them into **tokens**, the "words" of the language. It also discards everything the parser does not need: whitespace and comments.

```text
let x = 10 + 20 * 2;
   │
   ▼
LET  IDENTIFIER(x)  ASSIGN  NUMBER(10)  PLUS  NUMBER(20)  MUL  NUMBER(2)  SEMICOLON
```

Each token records its **kind**, its **lexeme** (the exact matched text) and its **position** (line and column). Later phases use the position in error messages.

## 3.2 Specification with Regular Expressions

Every token class is a regular language. The Flex specification [`parser/lexer.l`](../parser/lexer.l) lists one regular expression per class:

| Token | Regular expression |
|---|---|
| `IDENTIFIER` | `[A-Za-z_][A-Za-z0-9_]*` |
| `NUMBER` | `[0-9]+` |
| `LET` … `FALSE` | the literal strings `"let"`, `"if"`, … |
| `EQ`, `LE`, `AND`, … | `"=="`, `"<="`, `"&&"`, … |
| whitespace | `[ \t\r\f\v]+` and `\n` (no token) |
| line comment | `"//"[^\n]*` (no token) |
| block comment | `"/*" … "*/"` using a start condition (no token) |

Excerpt:

```lex
"let"                   { return TOK_LET; }
"while"                 { return TOK_WHILE; }
{IDENT}                 { return TOK_IDENTIFIER; }
{DIGIT}+                { return TOK_NUMBER; }
"=="                    { return TOK_EQ; }
"="                     { return TOK_ASSIGN; }
.                       { lexError("unexpected character " + ...); }
```

## 3.3 From Regular Expressions to a DFA

Flex converts the specification automatically, using the classic construction:

1. Each regular expression becomes an NFA (Thompson's construction).
2. The NFAs are joined with a new start state.
3. Subset construction turns the combined NFA into a DFA.
4. The DFA is minimized and emitted as C tables plus a driver loop (`lexer.yy.cpp`).

At run time the scanner walks the DFA one character at a time, which takes time linear in the input length.

## 3.4 Resolving Ambiguity

Several patterns can match at the same position. Flex applies two rules:

1. **Longest match.** For input `<=`, both `"<"` and `"<="` match, and the longer one wins, giving `LE`. For `whilex`, the identifier rule matches six characters while `"while"` matches only five, so it is an `IDENTIFIER`.
2. **First rule wins on ties.** For `while`, the keyword rule and the identifier rule both match five characters. The keyword rule is listed first, so the result is `WHILE`.

The test `tests/lexer_tests/longest_match.ml` checks both rules:

```text
a<=b a<b a==b a=b !a a!=b letter iffy whilex
→ IDENTIFIER LE IDENTIFIER IDENTIFIER LT IDENTIFIER IDENTIFIER EQ IDENTIFIER
  IDENTIFIER ASSIGN IDENTIFIER NOT IDENTIFIER IDENTIFIER NE IDENTIFIER
  IDENTIFIER IDENTIFIER IDENTIFIER
```

## 3.5 Position Tracking

Flex's `YY_USER_ACTION` macro runs before every action. It saves the current line and column as the token's start, then advances the column by the token length. The newline rule increments the line and resets the column. This gives exact positions such as `[Line 4, Col 11]` in error messages.

## 3.6 Integration with the Parser

Flex usually generates a function `yylex()` that the parser calls on demand. Here the scanner function is renamed `minilangScan()`, and `Lexer::tokenize()` calls it in a loop to build a complete `std::vector<Token>`. The parser's own `yylex()` (in `src/Parser.cpp`) then reads tokens from that vector. This has three benefits:

* Lexical analysis is a separate phase whose output can be printed (`--tokens`).
* Lexical errors are all reported before parsing starts.
* The token codes are those Bison generates (`TOK_LET`, …), so no translation table is needed.

## 3.7 Lexical Errors

| Input | Message |
|---|---|
| `x @ y` | `Lexer Error [Line 4, Col 11]: unexpected character '@'` |
| `a & b` | `unexpected character '&' (did you mean '&&'?)` |
| `a \| b` | `unexpected character '\|' (did you mean '\|\|'?)` |
| `2147483648` | `integer literal '2147483648' is too large (maximum is 2147483647)` |
| `2x` | `invalid numeric literal '2x' (identifiers cannot start with a digit)` |
| `"text"` | `string literals are not supported in MiniLang (only int and bool)` |
| `/* …` (no end) | `unterminated block comment '/*'` |

The lexer reports the error, skips the bad characters and **keeps scanning**, so all lexical errors appear in one run. The driver does not start the parser if any lexical error occurred.

## 3.8 Example Output (`--tokens`)

```text
LINE:COL  TOKEN         LEXEME
------------------------------------
1:1       LET           let
1:5       IDENTIFIER    x
1:7       ASSIGN        =
1:9       NUMBER        10
1:12      PLUS          +
1:14      NUMBER        20
1:17      MUL           *
1:19      NUMBER        2
1:20      SEMICOLON     ;
2:1       PRINT         print
...
Token stream (15 tokens):
  LET IDENTIFIER ASSIGN NUMBER PLUS NUMBER MUL NUMBER SEMICOLON
  PRINT LPAREN IDENTIFIER RPAREN SEMICOLON
  EOF
```
