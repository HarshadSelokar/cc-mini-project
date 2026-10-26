# 9. Testing

## 9.1 Strategy

Each compiler phase has its own folder of tests. Valid programs check that a phase produces the right result; invalid programs check that it rejects bad input with the right message. The end-to-end tests then compile and run real programs.

```bash
bash tests/run_tests.sh bin/minilang.exe     # direct
make test                                     # via make
ctest --test-dir build                        # via CMake/CTest
```

The current result is **71 passed, 0 failed**.

## 9.2 How a Test Is Described

The runner, [`tests/run_tests.sh`](../tests/run_tests.sh), finds every `NAME.ml` and decides what to check from the files next to it:

| Companion file | Check |
|---|---|
| `NAME.tokens` | output of `--tokens` must match exactly |
| `NAME.ast` | output of `--ast` must match exactly |
| `NAME.optsrc` | optimized program printed by `--optimized-ast` must match |
| `NAME.expected` | compile, **run**, and compare the program output, both optimized and with `-O0`; the `.ll` must pass `opt -passes=verify` |
| `NAME.err` | compilation must fail with exit status 1, and every line of `NAME.err` must appear in the error output |

Adding a test therefore means writing a `.ml` file and its expected-result file; the runner needs no changes. `bash tests/run_tests.sh bin/minilang.exe --update` regenerates golden files after an intentional change.

## 9.3 Test Catalogue

### Lexer tests (`tests/lexer_tests`)

| Test | Purpose |
|---|---|
| `all_tokens` | every keyword, identifier form, literal, operator and delimiter |
| `comments_whitespace` | `//` and `/* */` comments, tabs and spaces are skipped |
| `longest_match` | `<=` vs `<`, `==` vs `=`, keywords vs `iffy`, `whilex` |
| `invalid_character` | `@` → `Lexer Error [Line 4, Col 11]` |
| `single_ampersand` | `&` → hint to use `&&` |
| `integer_too_large` | `2147483648` rejected |
| `bad_identifier` | `2x` rejected |
| `unterminated_comment` | `/*` without `*/` |
| `string_literal` | `"hello"` → not supported |

### Parser tests (`tests/parser_tests`)

| Test | Purpose |
|---|---|
| `precedence` | `a + b * c`, `(a + b) * c`, mixed `* + % -` and unary minus, `&&` over `\|\|` |
| `associativity` | `a - 5 - 3`, `a / 2 / 5` are left-associative; `!true == false` |
| `statements` | AST of let, assign, print, if / else-if / else, while, nested block |
| `missing_semicolon` | reported at the end of the line missing the `;` |
| `malformed_expression` | `5 + ;` |
| `missing_paren`, `missing_brace` | unbalanced `(` and `{` |
| `invalid_operator` | `x <> 3` |
| `else_without_if`, `if_without_parens` | misplaced keywords |
| `empty_statement` | `;;` |
| `multiple_errors` | error recovery reports two errors in one run |

### Semantic tests (`tests/semantic_tests`)

| Test | Error detected |
|---|---|
| `undeclared_variable` | `count` used without `let` |
| `duplicate_declaration` | `x` declared twice in one scope |
| `type_mismatch` | `int + bool` |
| `invalid_assignment` | assigning `bool` to an `int` variable |
| `invalid_condition` | `if (x)` / `while (x - 1)` with an int condition |
| `invalid_operation` | `-bool`, `!int`, `bool < int`, `bool == int` |
| `division_by_zero` | `a / 0` |
| `out_of_scope` | variable used after its block ends |
| `multiple_errors` | four different errors reported together |
| `valid_shadowing` | **valid** program: shadowing and loop-local variables run correctly |

### Optimization tests (`tests/optimization_tests`)

Each test checks the optimized program (`.optsrc`) **and** that the output is unchanged. The expected outputs were generated with the optimizer disabled.

| Test | Technique |
|---|---|
| `constant_folding` | arithmetic, unary, relational and logical folding |
| `constant_propagation` | propagation through declarations and re-assignment |
| `algebraic_simplification` | all identity rules on a loop variable |
| `dead_code_elimination` | `if(false)`, `if(true)`, `while(false)`, conditions that become constant |
| `propagation_through_branches` | intersection of facts after if/else |
| `loop_safety` | loop-modified variables are not propagated |
| `overflow_wraparound` | folding matches 32-bit run-time arithmetic |

### Code generation tests (`tests/codegen_tests`)

| Test | Expected output |
|---|---|
| `example1_add` | `30` |
| `example2_folding` | `50` |
| `example3_ifelse` | `10` |
| `example4_while` | `3 2 1` |
| `all_operators` | all 12 binary operators and unary minus |
| `booleans` | printing and combining booleans |
| `short_circuit` | `d != 0 && 10 / d > 1` does not divide by zero |
| `nested_control` | nested loops with else-if chains |
| `empty_program` | a file containing only a comment produces valid IR |

## 9.4 Robustness Testing

The compiler was also given hostile input and must never crash:

| Input | Result |
|---|---|
| empty file | valid empty program |
| a single `}` | one parser error |
| 3 KB of random bytes | lexer errors (first 20 shown), exit status 1 |
| expression nested 3000 levels deep | compiles and prints `1` |
| 2000 variable declarations | compiles and runs |

## 9.5 Verifying the Generated IR Manually

```bash
minilang examples/optimization.ml --llvm
opt -passes=verify -disable-output output.ll && echo "IR is valid"
llvm-as output.ll -o output.bc
llc output.ll -o output.s
lli output.ll
```
