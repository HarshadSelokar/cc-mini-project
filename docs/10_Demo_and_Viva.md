# 10. Demonstration and Viva Guide

## 10.1 Preparation (before the presentation)

Open the **MSYS2 UCRT64** shell:

```bash
cd "/d/7th Sem/CC/cc-mini"        # adjust to your path
cmake -S . -B build && cmake --build build
export PATH="$PWD/bin:$PATH"
bash tests/run_tests.sh bin/minilang.exe   # should end with "71 passed, 0 failed"
```

## 10.2 Demo Script (about 10 minutes)

**Step 1. All stages at once.**

```bash
minilang examples/demo.ml --demo
```

This shows, in order: the source → tokens → AST → symbol table → optimizations and the optimized AST → LLVM IR → program output `50`. Explain each stage as it appears (see 10.3).

**Step 2. Precedence in the AST.**

```bash
minilang tests/parser_tests/precedence.ml --ast
```

Point out that in `a + b * c` the `*` node sits *below* the `+` node.

**Step 3. The optimizer at work.**

```bash
minilang examples/optimization.ml --optimized-ast
```

Walk through the log: folding `20 * 2 => 40`, propagation `a => 10`, simplification `n * 1 => n`, and dead code elimination of `if (false)` and `while (a > 100)`. Show the much shorter optimized program at the end.

**Step 4. LLVM IR and control flow.**

```bash
minilang examples/loops.ml --llvm -O0
```

Show the `while.cond`, `while.body` and `while.end` blocks, the `icmp` and the conditional `br`, and the `alloca`/`load`/`store` of variables.

**Step 5. Run a real program.**

```bash
minilang examples/primes.ml --run
minilang examples/optimization.ml --run
```

**Step 6. Error handling, one per phase.**

```bash
minilang tests/lexer_tests/invalid_character.ml        # Lexer Error
minilang tests/parser_tests/missing_semicolon.ml       # Parser Error
minilang tests/semantic_tests/undeclared_variable.ml   # Semantic Error
minilang tests/semantic_tests/type_mismatch.ml         # Semantic Error
```

Point out the phase name, the line and column, the caret, and that no IR is produced.

**Step 7. Verify the IR with LLVM's own tools.**

```bash
minilang examples/optimization.ml --llvm > /dev/null
opt -passes=verify -disable-output output.ll && echo valid
lli output.ll
```

## 10.3 What to Say at Each Stage

| Stage | Key points |
|---|---|
| Tokens | The Flex DFA groups characters into tokens using longest match and rule order; whitespace and comments vanish; each token keeps its line and column. |
| AST | Bison's LALR(1) parser builds the tree; precedence declarations resolve the ambiguous expression grammar; parentheses are not stored because the shape encodes them. |
| Symbol table | It is a stack of scopes; each declaration gets a unique id; types are inferred from initializers; errors stop the pipeline. |
| Optimization | Bottom-up folding, forward data-flow propagation, identities and dead branches are all applied on the AST, iterated to a fixed point, and preserve meaning. |
| LLVM IR | `alloca`/`load`/`store` for variables, `icmp` + `br` across basic blocks for control flow, `phi` for `&&`/`\|\|`, `printf` for output; the IR is verified before it is written. |
| Execution | Clang compiles the IR to x86-64 machine code and links it with the C runtime. |

## 10.4 Likely Viva Questions

**Q1. What are the phases of your compiler?**
Lexical analysis (Flex), syntax analysis (Bison, LALR(1)), AST construction, semantic analysis (symbol table, type checking), optimization (on the AST), and LLVM IR generation. Clang then produces the executable.

**Q2. What is the difference between a token, a lexeme and a pattern?**
The pattern is the rule (`[0-9]+`), the lexeme is the matched text (`42`), and the token is the class passed to the parser (`NUMBER`).

**Q3. How does the lexer decide between `<` and `<=`, or between `while` and an identifier?**
Longest match first (`<=` beats `<`, and `whilex` is an identifier). For equal lengths, the rule listed first wins, which is why keywords are listed before the identifier rule.

**Q4. What kind of parser does Bison generate?**
An LALR(1) bottom-up shift-reduce parser driven by an action/goto table, with one token of lookahead.

**Q5. How is operator precedence implemented?**
With `%left`, `%nonassoc` and `%precedence` declarations. They resolve the shift/reduce conflicts of the ambiguous expression grammar. Higher precedence of the lookahead means shift; equal precedence with `%left` means reduce. `%prec UMINUS` gives unary minus its own level.

**Q6. Why build an AST instead of generating code in the parser actions?**
It separates the phases. Semantic analysis and optimization need the whole program as a tree, and the same tree serves several passes (checking, optimizing, printing, code generation).

**Q7. What is a symbol table and how do you handle scopes?**
It maps names to information (type, id, position). It is a stack of hash maps; `{` pushes and `}` pops. Lookup searches from the innermost scope outwards, which implements shadowing. Duplicate checks look only at the innermost scope.

**Q8. Which semantic errors do you detect?**
Undeclared variables, duplicate declarations, type mismatches in operators, invalid assignments, non-boolean conditions, invalid unary operations, and division by the constant zero. Unused variables produce a warning.

**Q9. Explain constant folding versus constant propagation.**
Folding evaluates operators on constants (`20 * 2 → 40`). Propagation substitutes a variable's known constant value (`a → 10`). They feed each other: propagation creates new constants that folding can evaluate.

**Q10. How do you make sure propagation is correct with loops and branches?**
After an if/else, only facts that hold on both paths survive (intersection). Before a loop, every variable assigned inside the loop body is removed from the environment.

**Q11. Can the optimizer change the program's output?**
No. Folding uses the same 32-bit wrap-around arithmetic as the generated `i32` code, and it never folds undefined operations such as division by zero. The test suite runs every program with and without the optimizer and requires identical output.

**Q12. What is SSA? Your IR uses `alloca`; is it still SSA?**
In SSA every register is assigned once. Variables live in memory (`alloca`) and are accessed with `load`/`store`, so the registers themselves are still assigned once. LLVM's `mem2reg` pass can turn the memory slots into SSA registers with `phi` nodes.

**Q13. What is a basic block, and how are `if` and `while` translated?**
A basic block is straight-line code with one entry and one terminator. `if` becomes the blocks `if.then`, `if.else` and `if.end`, joined by a conditional `br`. `while` becomes `while.cond`, `while.body` and `while.end`, with a back edge from the body to the condition.

**Q14. What is a `phi` node, and where do you use one?**
A `phi` node selects a value depending on which predecessor block control came from. It implements the short-circuit `&&`/`||`: `phi i1 [false, %entry], [%rhs, %and.rhs]`.

**Q15. How does `print` work?**
The module declares the external C function `printf` and global format strings (`"%d\n"`, `"%s\n"`). `print(e)` becomes a `call` to `printf`. Booleans pick `"true"` or `"false"` with `select`.

**Q16. How do you know the IR is valid?**
`llvm::verifyModule` runs inside the compiler before the IR is written. The tests run `opt -passes=verify` on every output, and the IR has also been checked with `llvm-as`, `llc` and `lli`.

**Q17. Why LLVM?**
It provides a well-defined, typed IR and production-quality optimizers and code generators for many CPUs, so the front end only has to emit IR.

**Q18. What are the limitations, and how would you extend the language?**
There are no functions, arrays or strings. Functions would become separate LLVM `Function`s with parameters, `call` and `ret`, and symbol tables per function. Arrays would use LLVM array types and `getelementptr`.

**Q19. How does error recovery work in the parser?**
Panic mode: the production `statement: error ';'` lets Bison discard tokens up to the next `;` and continue, so several syntax errors are reported in one run.

**Q20. What happens if the input has errors? Is any IR produced?**
No. Each phase reports all the errors it finds, and the driver stops before the next phase. The compiler exits with status 1 and prints "No LLVM IR was generated" for semantic errors.
