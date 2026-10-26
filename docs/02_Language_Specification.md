# 2. MiniLang Language Specification

## 2.1 Overview

MiniLang is a small imperative language. A program is a sequence of statements executed from top to bottom. There are two types (`int`, `bool`), block-scoped variables, conditionals, loops and a `print` statement.

```text
let x = 3;
while (x > 0) {
    print(x);
    x = x - 1;
}
```

## 2.2 Lexical Structure

| Category | Definition | Examples |
|---|---|---|
| Keywords | `let if else while print true false` | `let` |
| Identifier | `[A-Za-z_][A-Za-z0-9_]*`, not a keyword | `x`, `counter`, `my_Var1` |
| Integer literal | `[0-9]+`, value ≤ 2147483647 | `0`, `10`, `999` |
| Arithmetic operators | `+ - * / %` | |
| Relational operators | `< > <= >= == !=` | |
| Logical operators | `&& \|\| !` | |
| Assignment | `=` | |
| Delimiters | `( ) { } ;` | |
| Comments | `// to end of line`, `/* ... */` | |
| Whitespace | space, tab, CR, LF | ignored |

Identifiers are case-sensitive. A negative number such as `-5` is the unary minus operator applied to the literal `5`.

## 2.3 Syntax (EBNF)

```ebnf
program        = { statement } ;

statement      = declaration | assignment | print_stmt
               | if_statement | while_stmt | block ;

declaration    = "let" IDENT "=" expression ";" ;
assignment     = IDENT "=" expression ";" ;
print_stmt     = "print" "(" expression ")" ";" ;
if_statement   = "if" "(" expression ")" block
                 [ "else" ( block | if_statement ) ] ;
while_stmt     = "while" "(" expression ")" block ;
block          = "{" { statement } "}" ;

expression     = or_expr ;
or_expr        = and_expr { "||" and_expr } ;
and_expr       = eq_expr  { "&&" eq_expr } ;
eq_expr        = rel_expr [ ( "==" | "!=" ) rel_expr ] ;
rel_expr       = add_expr [ ( "<" | ">" | "<=" | ">=" ) add_expr ] ;
add_expr       = mul_expr { ( "+" | "-" ) mul_expr } ;
mul_expr       = unary    { ( "*" | "/" | "%" ) unary } ;
unary          = ( "-" | "!" ) unary | primary ;
primary        = NUMBER | "true" | "false" | IDENT | "(" expression ")" ;
```

Notes:

* The bodies of `if`, `else` and `while` must be blocks `{ ... }`. This rules out the "dangling else" ambiguity. `else if` chains are allowed.
* Relational and equality operators are **non-associative**: `a < b < c` is a syntax error (write `a < b && b < c`).
* An empty statement (`;;`) is not allowed.

## 2.4 Types

| MiniLang | Values | LLVM type |
|---|---|---|
| `int` | −2,147,483,648 … 2,147,483,647 | `i32` |
| `bool` | `true`, `false` | `i1` |

A variable's type is **inferred** from its initializer and is **fixed** for its lifetime.

## 2.5 Typing Rules

Written as *operand types → result type*:

| Expression | Rule |
|---|---|
| integer literal | `int` |
| `true`, `false` | `bool` |
| variable `x` | type of the declaration of `x` |
| `e1 + e2`, `-`, `*`, `/`, `%` | `int × int → int` |
| `e1 < e2`, `>`, `<=`, `>=` | `int × int → bool` |
| `e1 == e2`, `!=` | `T × T → bool` (both operands the same type) |
| `e1 && e2`, `\|\|` | `bool × bool → bool` |
| `-e` | `int → int` |
| `!e` | `bool → bool` |

Statement rules:

* `let x = e;` declares `x` with the type of `e`. `x` must not already be declared **in the same scope**.
* `x = e;` requires `x` to be declared, and the type of `e` must equal the type of `x`.
* The conditions of `if` and `while` must have type `bool`.
* `print(e);` accepts `int` or `bool`.
* Dividing by the literal `0` (`x / 0`, `x % 0`) is a compile-time error.

## 2.6 Scope Rules

* The program body is the **global scope**. Every block `{ ... }`, including the bodies of `if`, `else` and `while`, opens a **new scope**.
* A name is visible from its declaration to the end of the enclosing block.
* An inner scope may declare a name that already exists in an outer scope (**shadowing**). Inside the inner block the new variable hides the outer one.
* In `let x = e;`, the expression `e` is checked **before** `x` is declared. So `let x = x + 1;` refers to an outer `x`, or is an error if there is none.

```text
let x = 1;
{
    let x = true;   // new variable, shadows the outer x
    print(x);       // true
}
print(x);           // 1
```

## 2.7 Dynamic Semantics

* Statements run in order. `while` re-evaluates its condition before every iteration.
* Integer arithmetic is 32-bit two's complement and **wraps** on overflow (`2147483647 + 1 = -2147483648`).
* `/` truncates toward zero and `%` takes the sign of the dividend (`-7 / 2 = -3`, `-7 % 2 = -1`).
* `&&` and `||` **short-circuit**: the right operand is evaluated only when needed. For example, `d != 0 && 10 / d > 1` is safe when `d` is 0.
* `print(e)` writes the value and a newline. Integers print in decimal, booleans as `true` / `false`.
* Division by zero at run time is undefined behaviour (on Windows it terminates the program).
