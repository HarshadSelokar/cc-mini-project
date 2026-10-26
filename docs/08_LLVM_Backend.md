# 8. LLVM IR Backend

## 8.1 What Is LLVM IR?

LLVM IR is a typed, low-level, target-independent assembly language in **Static Single Assignment (SSA)** form: each virtual register (`%name`) is assigned exactly once. A module contains global variables and functions. A function is a list of **basic blocks**: straight-line instruction sequences, each ending in exactly one terminator (`br`, `ret`).

A compiler front end that emits LLVM IR gets LLVM's optimizers and its code generators for x86, ARM, RISC-V and other targets without further work. Clang, Rust and Swift work this way, and so does this compiler.

## 8.2 Generating IR with the C++ API

[`src/LLVMCodeGen.cpp`](../src/LLVMCodeGen.cpp) is an AST visitor that uses the LLVM C++ API:

| LLVM class | Use |
|---|---|
| `LLVMContext` | owns types and constants |
| `Module` | the compilation unit (`output.ll`) |
| `IRBuilder<>` | creates instructions at an insertion point |
| `Function`, `BasicBlock` | `@main` and its blocks |
| `verifyModule` | checks the finished IR |

```cpp
llvm::Value* l = emit(*node.lhs);
llvm::Value* r = emit(*node.rhs);
lastValue_ = builder_->CreateAdd(l, r, "add");     //  %add = add i32 %l, %r
```

## 8.3 Translation Scheme

| MiniLang | LLVM IR |
|---|---|
| whole program | `define i32 @main() { entry: ... ret i32 0 }` |
| `int` / `bool` | `i32` / `i1` |
| `let x = e;` | `%x = alloca i32` in the entry block, then `store` |
| read `x` | `%x.val = load i32, ptr %x` |
| `x = e;` | `store i32 %e, ptr %x` |
| `+ - * / %` | `add sub mul sdiv srem` |
| `< > <= >= == !=` | `icmp slt / sgt / sle / sge / eq / ne` |
| `-e` / `!e` | `sub i32 0, %e` / `xor i1 %e, true` |
| `a && b`, `a \|\| b` | conditional branch + `phi` (short circuit) |
| `print(e)` | `call i32 (ptr, ...) @printf(ptr @.fmt.int, i32 %e)` |
| `if` / `while` | basic blocks + `br` |

### Variables: `alloca`, `load`, `store`

Building SSA form directly is hard when a variable is assigned in a loop. The standard technique, used by Clang at `-O0`, is to give every variable a **stack slot** (`alloca`) in the entry block and to access it with `load`/`store`. The IR is then trivially correct SSA, because only the addresses are SSA values. LLVM's `mem2reg` pass can later promote the slots to registers and insert `phi` nodes automatically.

Each declaration's **symbol id** maps to its `AllocaInst*`. Shadowed variables therefore get separate slots (`%x`, `%x1`).

## 8.4 Control Flow

**if / else**

```text
            br i1 %cond, label %if.then, label %if.else
  if.then:  <then statements>      br label %if.end
  if.else:  <else statements>      br label %if.end
  if.end:   ...
```

Without an `else` branch the false edge goes straight to `if.end`.

**while**

```llvm
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
```

The back edge `while.body → while.cond` forms the loop.

**Short-circuit `&&` with `phi`.** The right operand runs only if the left one is true. The result is merged with a `phi` node, which selects a value according to the predecessor block control came from:

```llvm
  %gt = icmp sgt i32 %a.val, 0
  br i1 %gt, label %and.rhs, label %and.end

and.rhs:                                          ; preds = %entry
  %b.val = load i32, ptr %b, align 4
  %lt = icmp slt i32 %b.val, 10
  br label %and.end

and.end:                                          ; preds = %and.rhs, %entry
  %and = phi i1 [ false, %entry ], [ %lt, %and.rhs ]
  br i1 %and, label %if.then, label %if.else
```

`||` is symmetric: the true edge skips the right operand and the `phi` yields `true`.

Blocks are created detached and appended to the function only when code generation reaches them, so the `.ll` file lists blocks in source order.

## 8.5 The `print` Runtime

`print` is implemented with the C library's `printf`, declared as a variadic external function, plus four private constant strings:

```llvm
@.fmt.int   = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@.fmt.str   = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@.str.true  = private unnamed_addr constant [5 x i8] c"true\00", align 1
@.str.false = private unnamed_addr constant [6 x i8] c"false\00", align 1

declare i32 @printf(ptr, ...)
```

* `print(int)` → `printf("%d\n", value)`
* `print(bool)` → `%s = select i1 %b, ptr @.str.true, ptr @.str.false`, then `printf("%s\n", %s)`

LLVM 15 and later use **opaque pointers**, so every pointer has the single type `ptr`, which is the `i8*` of older LLVM versions.

## 8.6 Target Triple

The module is tagged with the host triple from `llvm::sys::getDefaultTargetTriple()`, which is `x86_64-w64-windows-gnu` under MSYS2 UCRT64. Clang can then link it without warnings.

## 8.7 Verification

Before the IR is written, `llvm::verifyModule` checks it. Among other things, it confirms that every block ends in a terminator, that operand types match, that definitions dominate their uses, and that `phi` nodes list exactly their predecessors. If the check fails, the compiler reports an internal CodeGen error instead of writing broken IR.

The test suite verifies every generated file again with LLVM's own command-line tool:

```bash
opt -passes=verify -disable-output output.ll
```

The output was also checked with `llvm-as` (assembler), `llc` (native code generator) and `lli` (JIT/interpreter).

## 8.8 From IR to a Running Program

`--run` writes `output.ll` and then runs

```bash
clang output.ll -o output.exe -Wno-override-module
output.exe
```

Clang runs the LLVM back end (instruction selection, register allocation, x86-64 machine code) and links against the C runtime, which provides `printf`. The compiler finds `clang` in the LLVM tools directory reported by `llvm-config --bindir` at build time, or on `PATH`. If clang is missing it falls back to `lli output.ll`.
