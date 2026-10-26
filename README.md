# A Mini-Language Compiler with Optimizing Backend Targeting LLVM IR

A complete compiler, written in C++17, for **MiniLang**, a small imperative language with integers, booleans, variables, `if`/`else`, `while` and `print`.
The compiler covers every classic phase: lexical analysis (Flex), syntax analysis (Bison), AST construction, semantic analysis with a symbol table, an AST optimizer, and an LLVM IR backend built with the LLVM C++ API. The IR it produces is verified and turned into a native Windows executable with Clang.

```text
minilang examples/optimization.ml --run
```

---

## Contents

1. [Problem Statement](#problem-statement)
2. [Objectives](#objectives)
3. [Features](#features)
4. [Architecture](#architecture)
5. [Compiler Phases](#compiler-phases)
6. [Language Syntax](#language-syntax)
7. [Grammar](#grammar)
8. [Project Structure](#project-structure)
9. [MSYS2 Setup and Dependencies](#msys2-setup-and-dependencies)
10. [Building](#building)
11. [Running the Compiler](#running-the-compiler)
12. [Worked Example: All Stages](#worked-example-all-stages)
13. [Example Programs](#example-programs)
14. [Optimization Techniques](#optimization-techniques)
15. [Error Handling](#error-handling)
16. [Testing](#testing)
17. [Limitations](#limitations)
18. [Future Scope](#future-scope)
19. [Academic Documentation](#academic-documentation)

---

## Problem Statement

High-level source code cannot run directly on hardware. A compiler must check that a program is well formed and meaningful, and then translate it into a lower-level form. Production compilers such as GCC and Clang are too large to study end to end. This project builds a compiler that is small enough to read completely but still real. It handles a structured language, reports errors precisely, optimizes, and produces LLVM IR, the same intermediate representation used by Clang, Rust and Swift.

## Objectives

* Implement each compiler phase as a separate, clearly documented module.
* Use the standard tools of compiler construction: **Flex** for the scanner and **Bison** for an LALR(1) parser.
* Build an object-oriented **AST** and use the **Visitor pattern** to traverse it.
* Perform **semantic analysis** with a scoped **symbol table** and a type checker.
* Implement visible **machine-independent optimizations** on the AST.
* Generate **valid LLVM IR** with the LLVM C++ API, verify it, and execute it.
* Make every stage easy to show during a presentation or viva (`--demo`).

## Features

| Area | What is supported |
|---|---|
| Types | `int` (32-bit, LLVM `i32`), `bool` (LLVM `i1`) |
| Statements | `let` declaration, assignment, `print(...)`, `if`, `if-else`, `else if`, `while`, `{ blocks }` |
| Operators | `+ - * / %`, `< > <= >= == !=`, `&& \|\| !`, unary `-` |
| Semantics | Precedence and associativity, block scoping and shadowing, type inference from `let`, short-circuit `&&`/`\|\|` |
| Comments | `// line` and `/* block */` |
| Errors | Phase name, line, column, and a source excerpt with a caret; each phase checks its input before the next runs |
| Optimizer | Constant folding, constant propagation, algebraic simplification, dead code elimination |
| Backend | `alloca`/`load`/`store`, arithmetic, `icmp`, `br`, `phi`, basic blocks, `call @printf`, `ret` |
| Tooling | `--tokens --ast --semantic --optimized-ast --llvm --run --demo -O0`, CMake and Makefile builds, 71 automated tests |

## Architecture

```text
                    +-----------------------------------------------------------+
  program.ml  --->  |  FRONT END                                                |
                    |                                                           |
                    |  [1] Lexer (Flex)          characters  -> tokens          |
                    |        parser/lexer.l, src/Lexer.cpp                      |
                    |                 |                                         |
                    |                 v                                         |
                    |  [2] Parser (Bison LALR(1)) tokens     -> AST             |
                    |        parser/parser.y, src/Parser.cpp, src/AST.cpp       |
                    |                 |                                         |
                    |                 v                                         |
                    |  [3] Semantic Analyzer     AST -> typed, resolved AST     |
                    |        src/SemanticAnalyzer.cpp, src/SymbolTable.cpp      |
                    +-----------------|-----------------------------------------+
                                      v
                    +-----------------------------------------------------------+
                    |  MIDDLE END                                               |
                    |  [4] Optimizer             AST -> optimized AST           |
                    |        src/Optimizer.cpp                                  |
                    +-----------------|-----------------------------------------+
                                      v
                    +-----------------------------------------------------------+
                    |  BACK END                                                 |
                    |  [5] LLVM IR Generator     AST -> LLVM module -> output.ll|
                    |        src/LLVMCodeGen.cpp  (IRBuilder + verifyModule)    |
                    +-----------------|-----------------------------------------+
                                      v
                    [6] clang output.ll -> output.exe -> run   (--run)

      Diagnostics (src/Diagnostics.cpp) is shared by all phases; the driver
      (src/main.cpp) stops after any phase that reports an error.
```

## Compiler Phases

| # | Phase | Input → Output | Implementation |
|---|---|---|---|
| 1 | Lexical analysis | characters → tokens | Flex DFA in `parser/lexer.l`; `Lexer` collects tokens, checks literal range |
| 2 | Syntax analysis | tokens → AST | Bison LALR(1) grammar in `parser/parser.y`; actions build AST nodes |
| 3 | Semantic analysis | AST → annotated AST | `SemanticAnalyzer` visitor + scoped `SymbolTable`; type checking |
| 4 | Optimization | AST → smaller AST | `Optimizer`: folding, propagation, simplification, DCE to a fixed point |
| 5 | Code generation | AST → LLVM IR | `LLVMCodeGen` visitor using `llvm::IRBuilder`, checked by `llvm::verifyModule` |
| 6 | Execution | `.ll` → `.exe` | Driver calls `clang` (falls back to `lli`) |

## Language Syntax

```text
// declarations: the type is inferred from the initial value
let x = 10;              // int
let ok = x > 5;          // bool

// assignment (the type must stay the same)
x = x + 5;

// output: ints print as numbers, bools as true / false
print(x * 2);

// conditionals (braces are required)
if (x > 10) {
    print(1);
} else if (x > 5) {
    print(2);
} else {
    print(3);
}

// loops
while (x > 0) {
    x = x - 1;
}

// blocks create a new scope
{
    let x = true;        // shadows the outer x inside this block
    print(x);
}
```

**Operator precedence** (highest first):

| Level | Operators | Associativity |
|---|---|---|
| 1 | `!`  unary `-` | right (prefix) |
| 2 | `*  /  %` | left |
| 3 | `+  -` | left |
| 4 | `<  >  <=  >=` | non-associative |
| 5 | `==  !=` | non-associative |
| 6 | `&&` | left |
| 7 | `\|\|` | left |

So `2 + 3 * 4` is `2 + (3 * 4)` = 14, and `20 - 5 - 3` is `(20 - 5) - 3` = 12.

## Grammar

The grammar in EBNF. The Bison version is in [parser/parser.y](parser/parser.y).

```ebnf
program        = { statement } ;
statement      = "let" IDENT "=" expression ";"
               | IDENT "=" expression ";"
               | "print" "(" expression ")" ";"
               | if_statement
               | "while" "(" expression ")" block
               | block ;
if_statement   = "if" "(" expression ")" block [ "else" ( block | if_statement ) ] ;
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

## Project Structure

```text
cc-mini/
├── CMakeLists.txt            CMake build (uses llvm-config)
├── Makefile                  plain make build (uses llvm-config)
├── README.md
├── parser/
│   ├── lexer.l               Flex specification      (Phase 1)
│   └── parser.y              Bison grammar           (Phase 2)
├── include/                  headers, one per module
│   ├── Token.h  Lexer.h  Parser.h  AST.h  SymbolTable.h
│   ├── SemanticAnalyzer.h  Optimizer.h  LLVMCodeGen.h  Diagnostics.h
├── src/
│   ├── main.cpp              driver / command-line interface
│   ├── Lexer.cpp             Phase 1 driver around the Flex DFA
│   ├── Parser.cpp            Phase 2 driver around the Bison parser
│   ├── AST.cpp               AST nodes, tree printer, source printer
│   ├── SymbolTable.cpp       scoped symbol table
│   ├── SemanticAnalyzer.cpp  Phase 3: scope and type checking
│   ├── Optimizer.cpp         Phase 4: AST optimizations
│   ├── LLVMCodeGen.cpp       Phase 5: LLVM IR generation
│   └── Diagnostics.cpp       error / warning formatting
├── examples/                 sample MiniLang programs (*.ml)
├── tests/
│   ├── run_tests.sh          test runner
│   ├── lexer_tests/  parser_tests/  semantic_tests/
│   ├── optimization_tests/  codegen_tests/
├── docs/                     academic documentation (10 chapters)
├── output/                   generated .ll / .exe files
└── bin/minilang.exe          the compiler (after building)
```

## MSYS2 Setup and Dependencies

1. Install MSYS2 from <https://www.msys2.org> (for example into `C:\msys64`).
2. Open the **MSYS2 UCRT64** shell (Start menu → *MSYS2 UCRT64*). Run every command below in this shell.
3. Update and install the toolchain:

```bash
pacman -Syu                      # may close the window; reopen UCRT64 and run it again
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-llvm \
    mingw-w64-ucrt-x86_64-clang \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    flex bison make
```

4. Check the environment:

```bash
g++ --version          # tested with 16.2.0
flex --version         # 2.6.4
bison --version        # 3.8.2   (3.6 or newer required)
llvm-config --version  # 22.1.8
clang --version        # 22.1.8
cmake --version        # 4.4.2
```

`llvm-config` supplies the LLVM flags to both build systems:

```bash
llvm-config --cxxflags          # include paths and defines
llvm-config --ldflags           # library path
llvm-config --libs core support # -lLLVM-22
llvm-config --system-libs
```

## Building

**Option A: CMake** (recommended)

```bash
cd /c/path/to/cc-mini            # e.g. cd "/d/7th Sem/CC/cc-mini"
cmake -S . -B build
cmake --build build
```

**Option B: make**

```bash
make            # builds bin/minilang.exe
make check-env  # prints tool versions
```

Both produce `bin/minilang.exe`. To type `minilang` instead of `./bin/minilang`:

```bash
export PATH="$PWD/bin:$PATH"
```

> The compiler links to `libLLVM-22.dll`, so run it from the UCRT64 shell. To run it from PowerShell or cmd, first add `C:\msys64\ucrt64\bin` to `PATH`.

## Running the Compiler

```bash
minilang program.ml                  # compile -> output.ll
minilang program.ml --tokens         # Phase 1: token stream
minilang program.ml --ast            # Phase 2: abstract syntax tree
minilang program.ml --semantic       # Phase 3: symbol table
minilang program.ml --optimized-ast  # Phase 4: optimizations + optimized AST
minilang program.ml --llvm           # Phase 5: write output.ll and print it
minilang program.ml --run            # compile with clang and run
minilang program.ml --demo           # every stage, one after another
minilang program.ml --run -O0        # without the optimizer
minilang program.ml --llvm -o out/prog.ll
```

Options can be combined (`--ast --optimized-ast`). The compiler returns exit code 0 on success, 1 on compile errors and 2 for usage errors. With `--run` it returns the program's exit code.

Verifying the output with LLVM's own tools:

```bash
minilang examples/optimization.ml --llvm
opt -passes=verify -disable-output output.ll   # verifier
llvm-as output.ll -o output.bc                 # assembler
lli output.ll                                  # LLVM interpreter / JIT
clang output.ll -o program.exe && ./program.exe
```

## Worked Example: All Stages

`minilang examples/demo.ml --demo` prints every stage below. This is real output.

**Stage 1: source**

```text
let x = 10 + 20 * 2;
print(x);
```

**Stage 2: tokens**

```text
LET IDENTIFIER ASSIGN NUMBER PLUS NUMBER MUL NUMBER SEMICOLON
PRINT LPAREN IDENTIFIER RPAREN SEMICOLON
EOF
```

**Stage 3: AST.** `*` sits below `+` because it binds tighter.

```text
Program
|-- VarDecl 'x'
|   `-- BinaryExpr '+'
|       |-- IntegerLiteral 10
|       `-- BinaryExpr '*'
|           |-- IntegerLiteral 20
|           `-- IntegerLiteral 2
`-- Print
    `-- Variable 'x'
```

**Stage 4: symbol table**

```text
ID   NAME          TYPE   SCOPE  DECLARED  READS  WRITES
--------------------------------------------------------
#0   x             int    0      line 1    1      0
```

**Stage 5: optimization**

```text
  [constant-folding        ] line   1:  20 * 2  =>  40
  [constant-folding        ] line   1:  10 + 40  =>  50
  [constant-propagation    ] line   2:  x  =>  50

Optimized AST:
Program
|-- VarDecl 'x' : int
|   `-- IntegerLiteral 50 : int
`-- Print
    `-- IntegerLiteral 50 : int
```

**Stage 6: LLVM IR (`output.ll`)**

```llvm
target triple = "x86_64-w64-windows-gnu"

@.fmt.int = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@.fmt.str = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@.str.true = private unnamed_addr constant [5 x i8] c"true\00", align 1
@.str.false = private unnamed_addr constant [6 x i8] c"false\00", align 1

define i32 @main() {
entry:
  %x = alloca i32, align 4
  store i32 50, ptr %x, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @.fmt.int, i32 50)
  ret i32 0
}

declare i32 @printf(ptr, ...)
```

**Stage 7: execution**

```text
50
```

A `while` loop (`tests/codegen_tests/example4_while.ml`) shows the loop blocks:

```llvm
define i32 @main() {
entry:
  %x = alloca i32, align 4
  store i32 3, ptr %x, align 4
  br label %while.cond

while.cond:                                       ; preds = %while.body, %entry
  %x.val = load i32, ptr %x, align 4
  %gt = icmp sgt i32 %x.val, 0
  br i1 %gt, label %while.body, label %while.end

while.body:                                       ; preds = %while.cond
  %x.val1 = load i32, ptr %x, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @.fmt.int, i32 %x.val1)
  %x.val2 = load i32, ptr %x, align 4
  %sub = sub i32 %x.val2, 1
  store i32 %sub, ptr %x, align 4
  br label %while.cond

while.end:                                        ; preds = %while.cond
  ret i32 0
}
```

## Example Programs

| File | Shows | Output |
|---|---|---|
| `examples/demo.ml` | the viva example | `50` |
| `examples/basic.ml` | variables, assignment, print | `30 50 15 true` |
| `examples/arithmetic.ml` | all operators, precedence, associativity | `22 12 85 3 2 -12 14 20 12 5 86` |
| `examples/ifelse.ml` | if / else-if / else, logic, short circuit | `10 2 true true false 2` |
| `examples/loops.ml` | while, sum, nested loops | `3 2 1 55 10` |
| `examples/optimization.ml` | every optimization | `50 15 0 1 2 100` |
| `examples/factorial.ml` | 1! … 10! | `1 2 6 … 3628800` |
| `examples/fibonacci.ml` | first 15 Fibonacci numbers | `0 1 1 2 … 377` |
| `examples/primes.ml` | primes below 50 | `2 3 5 … 47` |
| `examples/fizzbuzz.ml` | else-if chains, `%` | `1 2 -3 4 -5 …` |
| `examples/scopes.ml` | block scoping, shadowing | `true 1 0 10 1` |

(Outputs are shown on one line here; the programs print one value per line.)

## Optimization Techniques

The optimizer rewrites the AST after semantic analysis. It repeats until nothing changes and logs every rewrite (`--optimized-ast`).

| Technique | Before | After |
|---|---|---|
| Constant folding | `let x = 10 + 20 * 2;` | `let x = 50;` |
| Constant propagation | `let a = 10; let b = a + 5;` | `let a = 10; let b = 15;` |
| Algebraic simplification | `x + 0`, `x * 1`, `x - 0`, `x * 0`, `x / 1`, `x % 1`, `x - x`, `b && true`, `b \|\| false`, `!!b` | `x`, `x`, `x`, `0`, `x`, `0`, `0`, `b`, `b`, `b` |
| Dead code elimination | `if (false) { … }`, `while (false) { … }`, `if (true) {A} else {B}` | removed, removed, `A` |

The optimizer is careful about correctness:

* **Flow-sensitive propagation:** after `if/else`, only facts true on *both* paths survive. Variables assigned inside a loop are never treated as constants within that loop.
* **Exact arithmetic:** folding uses 32-bit wrap-around, the same as the `i32` instructions, and never folds division by zero or `INT_MIN / -1`.
* The test suite runs every program **with and without** the optimizer and requires identical output.

Excerpt of `minilang examples/optimization.ml --optimized-ast`:

```text
  [constant-folding        ] line   5:  20 * 2  =>  40
  [constant-folding        ] line   5:  10 + 40  =>  50
  [constant-propagation    ] line  10:  a  =>  10
  [constant-folding        ] line  10:  10 + 5  =>  15
  [algebraic-simplification] line  16:  n * 1  =>  n
  [algebraic-simplification] line  17:  y * 0  =>  0
  [dead-code-elimination   ] line  23:  if (false) is always false: removed the whole statement
  [dead-code-elimination   ] line  29:  if (debug && true) is always true: kept 'then' branch, removed 'else' branch
  [dead-code-elimination   ] line  36:  while (a > 100) never executes: removed the loop
```

## Error Handling

Each message names the phase and the position, and quotes the source line:

```text
Lexer Error [Line 4, Col 11]: unexpected character '@'
    4 | let z = x @ y;
      |           ^

Parser Error [Line 1, Col 11]: expected ';' after '10' (found 'print')
    1 | let x = 10
      |           ^

Semantic Error [Line 2, Col 1]: variable 'count' is not declared (use 'let count = ...;' to declare it)
    2 | count = count + 1;
      | ^

Semantic Error [Line 3, Col 11]: invalid operands to '+': expected 'int' and 'int', but got 'int' and 'bool'
    3 | let y = x + flag;
      |           ^
```

* Every phase runs to completion and reports all the errors it finds. The pipeline stops before the next phase, so **no LLVM IR is generated when errors exist**.
* The parser recovers at the next `;`, so several syntax errors can be reported in one run.
* At most 20 errors are printed. Invalid input never crashes the compiler, including random binary data and expressions nested 3000 levels deep.

## Testing

```bash
bash tests/run_tests.sh bin/minilang.exe     # or: make test   or: ctest --test-dir build
```

There are 71 checks across five folders, one per phase:

| Folder | What is checked |
|---|---|
| `lexer_tests` | token streams (golden files), invalid characters, `&`, out-of-range literals, unterminated comments, strings |
| `parser_tests` | AST shape for precedence/associativity/statements; missing `;`, `)`, `}`, malformed expressions, invalid operators, multiple errors |
| `semantic_tests` | undeclared variables, duplicates, type mismatch, invalid assignment, non-bool conditions, division by zero, scope rules |
| `optimization_tests` | expected optimized program **and** identical output with/without the optimizer |
| `codegen_tests` | the four required examples plus booleans, short circuit, all operators, nesting; IR verified with `opt -passes=verify` |

## Limitations

* Only `int` (32-bit) and `bool`; no strings, arrays, floats or user-defined types.
* No functions, `return`, `break`/`continue` or input; the program is one `main`.
* Integer overflow wraps around. Division by zero at run time is not checked and crashes the program (division by the literal `0` is a compile error).
* Variables must be initialized when declared, and their type cannot change.
* `--run` requires `clang` (or `lli`) and the MSYS2 runtime DLLs on `PATH`.

## Future Scope

* Functions with parameters and recursion (`define`, `call`, `ret` per function).
* `for`, `break`, `continue`, and `read()` for input.
* Arrays and strings (LLVM aggregate types and GEP).
* Run LLVM's pass pipeline (`mem2reg`, `instcombine`, `gvn`) and compare it with the AST optimizer.
* SSA-based data-flow optimizations: dead store elimination and loop-invariant code motion.
* Generating object files directly through `TargetMachine`, and a JIT mode with ORC.
* Better diagnostics: "did you mean" suggestions for misspelled variables.

## Academic Documentation

Detailed chapters for each phase are in [docs/](docs/):

| Chapter | Topic |
|---|---|
| [01_Project_Overview.md](docs/01_Project_Overview.md) | Goals, architecture, design decisions |
| [02_Language_Specification.md](docs/02_Language_Specification.md) | Lexical rules, grammar, typing rules, semantics |
| [03_Lexical_Analysis.md](docs/03_Lexical_Analysis.md) | Regular expressions, Flex, DFA, lexical errors |
| [04_Syntax_Analysis.md](docs/04_Syntax_Analysis.md) | CFG, LALR(1), precedence, error recovery |
| [05_AST_Design.md](docs/05_AST_Design.md) | Node hierarchy, ownership, Visitor pattern |
| [06_Semantic_Analysis.md](docs/06_Semantic_Analysis.md) | Symbol table, scoping, type checking |
| [07_Optimization.md](docs/07_Optimization.md) | Folding, propagation, simplification, DCE |
| [08_LLVM_Backend.md](docs/08_LLVM_Backend.md) | LLVM IR, IRBuilder, control flow, printf |
| [09_Testing.md](docs/09_Testing.md) | Test strategy and test catalogue |
| [10_Demo_and_Viva.md](docs/10_Demo_and_Viva.md) | Demo script and likely viva questions |
