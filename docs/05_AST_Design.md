# 5. Abstract Syntax Tree Design

## 5.1 Parse Tree versus AST

A **parse tree** contains every grammar symbol, including parentheses, semicolons and chain rules. An **abstract syntax tree** keeps only what matters for meaning. In `(2 + 3) * 4`, the parentheses do not appear in the AST: the *shape* of the tree already says that the addition happens first.

```text
(2 + 3) * 4            BinaryExpr '*'
                       |-- BinaryExpr '+'
                       |   |-- IntegerLiteral 2
                       |   `-- IntegerLiteral 3
                       `-- IntegerLiteral 4
```

## 5.2 Class Hierarchy

Defined in [`include/AST.h`](../include/AST.h):

```text
ASTNode                       (loc: line/column; accept(); dump())
├── Expression                (kind, type)
│   ├── IntegerLiteral        value
│   ├── BooleanLiteral        value
│   ├── VariableExpression    name, symbolId
│   ├── BinaryExpression      op, lhs, rhs
│   └── UnaryExpression       op, operand
├── Statement                 (kind)
│   ├── VariableDeclaration   name, init, varType, symbolId
│   ├── Assignment            name, value, symbolId
│   ├── PrintStatement        value
│   ├── IfStatement           condition, thenBranch, elseBranch (Block | If | null)
│   ├── WhileStatement        condition, body
│   └── BlockStatement        statements
└── Program                   statements
```

Design points:

* **Ownership:** children are held in `std::unique_ptr`. The `Program` owns the whole tree, and destroying it frees everything without leaks or double frees. Nodes cannot be copied.
* **Kinds:** `Expression::kind` and `Statement::kind` let the optimizer check node types cheaply, without `dynamic_cast`.
* **Source locations** on every node give later phases precise error positions.
* **Operators** are enums (`BinaryOp::Add`, `UnaryOp::Not`, …) rather than strings.
* `else if` is represented naturally: the `elseBranch` of an `IfStatement` is another `IfStatement`.

## 5.3 Annotations Added by Later Phases

The parser fills in the syntactic fields. The semantic analyzer then **decorates** the tree:

| Field | Meaning |
|---|---|
| `Expression::type` | `int` or `bool` |
| `VariableDeclaration::varType` | inferred type of the variable |
| `*::symbolId` | unique id of the declaration a name refers to |

The `symbolId` matters when variables are shadowed. Both variables below are named `x` but get ids `#0` and `#1`, so the optimizer and code generator never mix them up:

```text
let x = 1;          // #0
{ let x = true; }   // #1
```

## 5.4 The Visitor Pattern

Several phases walk the same tree: semantic analysis, code generation and printing. Rather than adding a `check()`, `codegen()` and `print()` method to every node class, the AST uses the **Visitor** pattern:

```cpp
class ASTVisitor {
public:
    virtual void visit(IntegerLiteral&) = 0;
    virtual void visit(BinaryExpression&) = 0;
    virtual void visit(IfStatement&) = 0;
    // ... one overload per node class
};

void BinaryExpression::accept(ASTVisitor& v) { v.visit(*this); }   // double dispatch
```

Each phase is one class that implements `ASTVisitor`:

| Visitor | Purpose |
|---|---|
| `TreePrinter` (AST.cpp) | ASCII tree for `--ast` |
| `SemanticAnalyzer` | scope and type checking |
| `LLVMCodeGen` | LLVM IR generation |

Adding a new phase means adding a new visitor class; the node classes do not change.

The **optimizer** works differently. It *replaces* nodes (for example a whole `BinaryExpression` becomes an `IntegerLiteral`), so it is written as a function `ExprPtr optimizeExpr(ExprPtr)` that takes ownership of a subtree and returns a possibly different one. A `void visit()` method cannot express that naturally.

## 5.5 Printing the AST

`ASTNode::dump(std::ostream&)` (used by `--ast` and `--optimized-ast`) renders an indented tree. Types are shown once semantic analysis has run:

```text
Program
|-- VarDecl 'x'
|   `-- IntegerLiteral 10
`-- If
    |-- Condition
    |   `-- BinaryExpr '>'
    |       |-- Variable 'x'
    |       `-- IntegerLiteral 5
    |-- Then
    |   `-- Block
    |       `-- Print
    |           `-- Variable 'x'
    `-- Else
        `-- Block
            `-- Print
                `-- IntegerLiteral 0
```

A second printer, `printSource()`, turns an AST back into MiniLang source with the minimum number of parentheses. It is used to show the optimized program:

```text
let x = 50;
print(50);
```
