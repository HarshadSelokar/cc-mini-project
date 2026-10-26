# 7. Optimization

## 7.1 Goal and Placement

The optimizer ([`src/Optimizer.cpp`](../src/Optimizer.cpp)) runs **after semantic analysis**, when every expression has a type and every variable is resolved, and **before code generation**. It rewrites the AST into a smaller, faster program **with exactly the same observable behaviour**. These are *machine-independent* optimizations: they do not depend on the target CPU.

Run `minilang file.ml --optimized-ast` to see every transformation, the optimized AST and the optimized program as source. Use `-O0` to disable the optimizer.

## 7.2 Constant Folding

Evaluate at compile time any operator whose operands are all constants. Expressions are processed **bottom-up**, so folded children enable folding of their parents:

```text
let x = 10 + 20 * 2;
  [constant-folding] 20 * 2   =>  40
  [constant-folding] 10 + 40  =>  50
let x = 50;
```

It works for every operator: `3 < 4 && !(2 == 3)` folds to `true`.

**Correctness rules:**

* Arithmetic is done with **32-bit wrap-around**, exactly matching the `i32` `add`/`sub`/`mul` that would run otherwise (`2147483647 + 1 => -2147483648`, `65536 * 65536 => 0`).
* `/` and `%` use C++ semantics (truncate toward zero), which are the same as LLVM's `sdiv`/`srem`.
* `x / 0` and `INT_MIN / -1` are **not** folded, because they are undefined behaviour at run time. The optimizer issues a warning instead.

## 7.3 Constant Propagation

Replace a variable with its value when the value is known at that point:

```text
let a = 10;
let b = a + 5;        [constant-propagation] a => 10, [constant-folding] 10 + 5 => 15
print(b);             [constant-propagation] b => 15
```

This is a **forward data-flow analysis** over the AST. The analysis state is an environment `env : symbolId → constant`:

| Construct | Transfer function |
|---|---|
| `let x = c;` / `x = c;` (c a literal after folding) | `env[x] = c` |
| `let x = e;` / `x = e;` (e not constant) | remove `x` from `env` |
| `if (c) A else B` | analyse A and B on separate copies of `env`; afterwards keep only facts **equal on both paths** (intersection = the *meet* operator) |
| `while (c) B` | remove every variable **assigned anywhere in B** from `env` before analysing `c` and `B`; such variables may differ on every iteration |

Using symbol ids rather than names keeps propagation correct when variables are shadowed.

Examples from the tests:

```text
// propagation_through_branches.ml
if (n == 0) { a = 2; b = 3; } else { a = 2; b = 4; }
print(a);     // => print(2)   (a == 2 on both paths)
print(b);     // unchanged     (3 or 4)

// loop_safety.ml
while (i < 5) { total = total + step; i = i + 1; }
                 // step is never assigned in the loop -> total = total + 2
                 // total and i are assigned -> never propagated inside the loop
```

## 7.4 Algebraic Simplification

Apply identities that hold for **every** value of `x`. MiniLang expressions have no side effects, so dropping an operand is safe.

| Rule | Rule | Rule |
|---|---|---|
| `x + 0 → x`, `0 + x → x` | `x * 1 → x`, `1 * x → x` | `x * 0 → 0`, `0 * x → 0` |
| `x - 0 → x` | `x - x → 0` | `0 - x → -x` |
| `x / 1 → x` | `x % 1 → 0` | `-(-x) → x`, `!!b → b` |
| `b && true → b` | `b && false → false` | `b \|\| false → b` |
| `b \|\| true → true` | `x == x → true` | `x < x → false` |

```text
let y = n * 1 + 0;      =>  let y = n;
let z = (y - 0) * 0;    =>  let z = 0;
```

## 7.5 Dead Code Elimination

Remove code that can never run, or that has no effect:

| Pattern | Result |
|---|---|
| `if (false) { A }` | removed |
| `if (false) { A } else { B }` | `B` |
| `if (true) { A } else { B }` | `A` |
| `while (false) { A }` | removed |
| `if (c) { }` with no else | removed (`c` has no side effects) |
| empty `else { }` | removed |
| nested block that declares nothing | its statements are merged into the enclosing block |

Combined with propagation and folding, this removes branches whose condition only *becomes* constant after optimization:

```text
let debug = 2 > 1;           // folds to true
if (debug && true) {         // propagation + folding => if (true)
    print(x * 2);            // => print(100)
} else {
    print(-1);               // removed
}
```

## 7.6 Driver: Iterating to a Fixed Point

All four techniques run together in **one forward walk** over the program, so a fold can immediately enable a propagation and vice versa. The walk is repeated until a pass makes no change (at most 10 passes). In practice the second pass confirms that the fixed point has been reached.

## 7.7 Full Example

`minilang examples/optimization.ml --optimized-ast` reports:

```text
Summary: 7 constant folding, 7 constant propagation, 5 algebraic simplification,
         3 dead code elimination  (2 passes)
```

Before (36 lines of source) → after:

```text
let x = 50;
print(50);
let a = 10;
let b = 15;
print(15);
let n = 0;
while (n < 3) {
    let y = n;
    let z = 0;
    print(y);
    n = n + 1;
}
let debug = true;
print(100);
```

## 7.8 Validation

* `tests/optimization_tests/*.optsrc` hold the expected optimized program for each technique.
* For **every** test program that runs, the test suite compares the output of the optimized build with the output of the `-O0` build. The optimization tests' expected outputs were themselves produced with `-O0`, so the optimizer is checked against the unoptimized compiler.

## 7.9 Effect on the Generated Code

Without the optimizer (`-O0`), `let x = 10 + 20 * 2; print(x);` loads `x` at run time and passes it to printf (LLVM's IRBuilder already folds the literal arithmetic itself). With the optimizer, `print(x)` becomes `print(50)` at the AST level, so the IR has no load at all:

```llvm
  %x = alloca i32, align 4
  store i32 50, ptr %x, align 4
  %0 = call i32 (ptr, ...) @printf(ptr @.fmt.int, i32 50)
```
