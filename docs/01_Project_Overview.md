# 1. Project Overview

## 1.1 Title

**A Mini-Language Compiler with Optimizing Backend Targeting LLVM IR**

## 1.2 Motivation

A compiler is a pipeline of translations. Each step takes a representation of the program and produces a more structured or lower-level one:

```text
characters -> tokens -> syntax tree -> checked tree -> optimized tree -> LLVM IR -> machine code
```

Textbooks describe each step separately. This project builds all of them for one small language, so the hand-off between phases can be seen in working code. The target is **LLVM IR**, the intermediate representation behind Clang, Rust and Swift. As a result, the compiler's output can be inspected, verified and executed with industrial tools.

## 1.3 Objectives

1. Design a small structured language, **MiniLang**, with integers, booleans, variables, conditionals, loops and output.
2. Build the front end with the classic generator tools: **Flex** (scanner) and **Bison** (LALR(1) parser).
3. Represent programs as an **Abstract Syntax Tree (AST)** and traverse it with the **Visitor** design pattern.
4. Check scope and type rules in a **semantic analysis** phase backed by a **symbol table**.
5. Implement visible, provably safe **optimizations** on the AST.
6. Generate **valid LLVM IR** with the LLVM C++ API, verify it, and run it as a native program.
7. Report errors with the phase, the line and column, and a helpful message, and never crash on bad input.

## 1.4 Architecture

```text
        +---------+   tokens   +--------+   AST   +----------+  typed AST  +-----------+
source ->| Lexer   |----------->| Parser |-------->| Semantic |------------>| Optimizer |
        | (Flex)  |            | (Bison)|         | Analyzer |             |           |
        +---------+            +--------+         +----------+             +-----------+
                                                       |                         |
                                                  SymbolTable              optimized AST
                                                                                 v
                                     output.exe <-- clang <-- output.ll <-- +-----------+
                                                                            | LLVM IR   |
                                                                            | Generator |
                                                                            +-----------+
```

* **Front end** (phases 1–3) is language dependent: it understands MiniLang.
* **Middle end** (phase 4) improves the program without changing its meaning.
* **Back end** (phase 5) is target-oriented: it emits LLVM IR. LLVM and Clang then handle instruction selection, register allocation and linking for x86-64 Windows.

## 1.5 Module Map

| Module | Files | Responsibility |
|---|---|---|
| Diagnostics | `Diagnostics.h/.cpp` | Uniform error/warning messages with source excerpts |
| Lexer | `parser/lexer.l`, `Lexer.h/.cpp`, `Token.h` | Characters → tokens |
| Parser | `parser/parser.y`, `Parser.h/.cpp` | Tokens → AST |
| AST | `AST.h/.cpp` | Node classes, Visitor interface, printers |
| Symbol table | `SymbolTable.h/.cpp` | Scoped name → symbol mapping |
| Semantic analyzer | `SemanticAnalyzer.h/.cpp` | Scope and type checking, AST annotation |
| Optimizer | `Optimizer.h/.cpp` | Folding, propagation, simplification, DCE |
| Code generator | `LLVMCodeGen.h/.cpp` | AST → LLVM module, verification, `.ll` output |
| Driver | `main.cpp` | Command line, phase sequencing, invoking clang |

## 1.6 Key Design Decisions

| Decision | Reason |
|---|---|
| Flex and Bison generate the scanner and parser | They are the standard compiler-construction tools, and the specification files read like the formal definitions. |
| The lexer produces a complete token vector first | The lexer and parser stay separate phases, and `--tokens` can show exactly what the parser receives. |
| Parser actions only build the AST | Code generation directly in the parser would mix phases and make optimization impossible. |
| Every declaration gets a unique symbol id | Later phases never confuse shadowed variables that share a name. |
| Optimize on the AST, not with LLVM passes | The optimizations become visible, explainable and testable ("before / after"). |
| Variables live in `alloca` stack slots | This is the standard, simple way to produce correct IR without building SSA by hand. LLVM's `mem2reg` can promote the slots later. |
| `print` uses C `printf` | The output is a normal native program; no custom runtime library is needed. |
| The IR is verified before it is written | `llvm::verifyModule` guarantees that only valid IR leaves the compiler. |

## 1.7 Environment

The project was built and tested on **Windows 11** with **MSYS2 UCRT64**:

| Tool | Version |
|---|---|
| g++ | 16.2.0 |
| flex | 2.6.4 |
| bison | 3.8.2 |
| LLVM / clang | 22.1.8 |
| CMake | 4.4.2 |

The README contains the installation and build commands.
