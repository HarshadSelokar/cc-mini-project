# 6. Semantic Analysis

## 6.1 Why a Separate Phase?

A context-free grammar cannot express rules such as "a variable must be declared before it is used" or "`+` needs two integers". The statement `y = x + true;` is grammatically correct but meaningless. **Semantic analysis** checks these *context-sensitive* rules by walking the AST with knowledge of declarations and types.

## 6.2 The Symbol Table

[`SymbolTable`](../src/SymbolTable.cpp) is a **stack of hash maps**, one map per open scope:

```text
let x = 1;               scopes: [ {x:#0} ]
{                        scopes: [ {x:#0}, {} ]
    let x = true;        scopes: [ {x:#0}, {x:#1} ]
    let y = 2;           scopes: [ {x:#0}, {x:#1, y:#2} ]
    print(x);            lookup x -> #1 (innermost scope first)
}                        scopes: [ {x:#0} ]          (inner scope popped)
print(x);                lookup x -> #0
```

| Operation | Behaviour |
|---|---|
| `enterScope()` / `exitScope()` | push / pop a map at `{` / `}` |
| `declare(name, type)` | add to the innermost map; fails if the name is already there |
| `lookup(name)` | search from the innermost map outwards |
| `lookupCurrentScope(name)` | search the innermost map only (duplicate check) |

Each symbol records an id, name, type, scope depth, declaration position, and counts of reads and writes. `--semantic` prints the table:

```text
ID   NAME          TYPE   SCOPE  DECLARED  READS  WRITES
--------------------------------------------------------
#0   x             int    0      line 1    1      0
```

## 6.3 Checks Performed

| Check | Example | Message |
|---|---|---|
| Undeclared variable | `count = count + 1;` | `variable 'count' is not declared (use 'let count = ...;' to declare it)` |
| Duplicate declaration | `let x = 10; let x = 30;` | `duplicate declaration: variable 'x' is already declared in this scope (at line 1)` |
| Type mismatch in an operator | `x + flag` | `invalid operands to '+': expected 'int' and 'int', but got 'int' and 'bool'` |
| Comparing different types | `b == 1` | `type mismatch: cannot compare 'bool' with 'int' using '=='` |
| Invalid unary operation | `-b`, `!5` | `invalid operand to unary '-': expected 'int', but got 'bool'` |
| Invalid assignment | `x = true;` (x is int) | `invalid assignment: cannot assign a value of type 'bool' to variable 'x' of type 'int'` |
| Non-boolean condition | `if (x) {...}` | `condition of 'if' must be of type 'bool', but has type 'int' (hint: compare it, e.g. 'x != 0')` |
| Division by constant zero | `a / 0` | `division by zero` |
| Out of scope | use of `y` after its block | `variable 'y' is not declared` |
| Unused variable (warning) | `let t = 5;` never read | `Semantic Warning: variable 't' is declared but never used` |

## 6.4 Type Checking Algorithm

The analyzer is a visitor that computes types **bottom-up** (a synthesized attribute, in attribute-grammar terms):

```text
check(IntegerLiteral)  = int
check(BooleanLiteral)  = bool
check(Variable x)      = lookup(x).type        (error if not found)
check(l op r):
    tl = check(l); tr = check(r)
    arithmetic  : require tl = tr = int    -> int
    relational  : require tl = tr = int    -> bool
    equality    : require tl = tr          -> bool
    logical     : require tl = tr = bool   -> bool
```

The result is stored in `Expression::type`. The code generator later uses it to pick `i32` or `i1`, and to choose the `printf` format.

**Avoiding cascades.** When a subexpression has an error, its type becomes `Error`, and operators with an `Error` operand stay silent. An undeclared name is reported only once. A declaration whose initializer has an error still declares the variable, so later uses do not produce extra "not declared" messages. One mistake gives one message.

## 6.5 Declarations and Scope Subtleties

* In `let x = e;` the initializer `e` is checked **before** `x` is added to the table. So `let x = x + 1;` cannot refer to itself.
* Blocks, including `if`/`while` bodies, open a scope, which allows shadowing.
* Each declaration's unique **symbol id** is written into the AST (`VariableDeclaration::symbolId`, `VariableExpression::symbolId`, `Assignment::symbolId`). After this phase no other phase needs to resolve names again.

## 6.6 Stopping the Pipeline

`SemanticAnalyzer::analyze()` returns `false` if any error was reported. The driver then prints

```text
Compilation failed: 1 semantic error(s). No LLVM IR was generated.
```

and exits with status 1. The optimizer and code generator therefore only ever see a correct, fully typed AST. This is why the code generator needs no error checks of its own.
