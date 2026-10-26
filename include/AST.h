// =============================================================================
//  AST.h  --  Abstract Syntax Tree for MiniLang
// -----------------------------------------------------------------------------
//  The parser builds this tree; every later phase works on it:
//
//      ASTNode
//      +-- Expression
//      |   +-- IntegerLiteral       10
//      |   +-- BooleanLiteral       true / false
//      |   +-- VariableExpression   x
//      |   +-- BinaryExpression     a + b, a < b, a && b, ...
//      |   +-- UnaryExpression      -a, !a
//      +-- Statement
//      |   +-- VariableDeclaration  let x = e;
//      |   +-- Assignment           x = e;
//      |   +-- PrintStatement       print(e);
//      |   +-- IfStatement          if (c) { ... } else { ... }
//      |   +-- WhileStatement       while (c) { ... }
//      |   +-- BlockStatement       { s1; s2; ... }
//      +-- Program                  list of top-level statements
//
//  Ownership: every child is held by std::unique_ptr, so the whole tree is
//  freed automatically when the Program is destroyed.
//
//  Annotations filled in by semantic analysis:
//      Expression::type          (int or bool)
//      VariableDeclaration::varType / symbolId
//      VariableExpression::symbolId, Assignment::symbolId
//  symbolId uniquely identifies a variable even when an inner block shadows
//  an outer variable with the same name; the optimizer and the LLVM backend
//  use it instead of the name.
//
//  Phases traverse the tree with the Visitor pattern (ASTVisitor below).
// =============================================================================
#pragma once

#include "Diagnostics.h"

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace minilang {

// ---------------------------------------------------------------- types ----
enum class Type { Unknown, Int, Bool, Error };
const char* typeName(Type type);

enum class BinaryOp { Add, Sub, Mul, Div, Mod, Lt, Gt, Le, Ge, Eq, Ne, And, Or };
enum class UnaryOp { Neg, Not };

const char* opSymbol(BinaryOp op);
const char* opSymbol(UnaryOp op);
bool isArithmetic(BinaryOp op);   // + - * / %
bool isRelational(BinaryOp op);   // < > <= >=
bool isEquality(BinaryOp op);     // == !=
bool isLogical(BinaryOp op);      // && ||

class ASTVisitor;

// ----------------------------------------------------------------- base ----
class ASTNode {
public:
    explicit ASTNode(SourceLoc loc) : loc(loc) {}
    virtual ~ASTNode() = default;
    ASTNode(const ASTNode&) = delete;
    ASTNode& operator=(const ASTNode&) = delete;

    virtual void accept(ASTVisitor& visitor) = 0;

    // Print this subtree as an indented ASCII tree (for debugging / demos).
    void dump(std::ostream& out);

    SourceLoc loc;
};

// ---------------------------------------------------------- expressions ----
class Expression : public ASTNode {
public:
    enum class Kind { IntegerLiteral, BooleanLiteral, Variable, Binary, Unary };

    Expression(Kind kind, SourceLoc loc) : ASTNode(loc), kind(kind) {}

    const Kind kind;
    Type type = Type::Unknown;      // set by the semantic analyzer
};
using ExprPtr = std::unique_ptr<Expression>;

class IntegerLiteral : public Expression {
public:
    IntegerLiteral(SourceLoc loc, int32_t value)
        : Expression(Kind::IntegerLiteral, loc), value(value) {}
    void accept(ASTVisitor& visitor) override;

    int32_t value;
};

class BooleanLiteral : public Expression {
public:
    BooleanLiteral(SourceLoc loc, bool value)
        : Expression(Kind::BooleanLiteral, loc), value(value) {}
    void accept(ASTVisitor& visitor) override;

    bool value;
};

class VariableExpression : public Expression {
public:
    VariableExpression(SourceLoc loc, std::string name)
        : Expression(Kind::Variable, loc), name(std::move(name)) {}
    void accept(ASTVisitor& visitor) override;

    std::string name;
    int symbolId = -1;              // resolved declaration (semantic phase)
};

class BinaryExpression : public Expression {
public:
    BinaryExpression(SourceLoc loc, BinaryOp op, ExprPtr lhs, ExprPtr rhs)
        : Expression(Kind::Binary, loc), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
    void accept(ASTVisitor& visitor) override;

    BinaryOp op;
    ExprPtr lhs;
    ExprPtr rhs;
};

class UnaryExpression : public Expression {
public:
    UnaryExpression(SourceLoc loc, UnaryOp op, ExprPtr operand)
        : Expression(Kind::Unary, loc), op(op), operand(std::move(operand)) {}
    void accept(ASTVisitor& visitor) override;

    UnaryOp op;
    ExprPtr operand;
};

// ----------------------------------------------------------- statements ----
class Statement : public ASTNode {
public:
    enum class Kind { VarDecl, Assign, Print, If, While, Block };

    Statement(Kind kind, SourceLoc loc) : ASTNode(loc), kind(kind) {}

    const Kind kind;
};
using StmtPtr = std::unique_ptr<Statement>;
using StmtList = std::vector<StmtPtr>;

class VariableDeclaration : public Statement {
public:
    VariableDeclaration(SourceLoc loc, std::string name, ExprPtr init)
        : Statement(Kind::VarDecl, loc), name(std::move(name)), init(std::move(init)) {}
    void accept(ASTVisitor& visitor) override;

    std::string name;
    ExprPtr init;
    Type varType = Type::Unknown;   // inferred from the initializer
    int symbolId = -1;
};

class Assignment : public Statement {
public:
    Assignment(SourceLoc loc, std::string name, ExprPtr value)
        : Statement(Kind::Assign, loc), name(std::move(name)), value(std::move(value)) {}
    void accept(ASTVisitor& visitor) override;

    std::string name;
    ExprPtr value;
    int symbolId = -1;
};

class PrintStatement : public Statement {
public:
    PrintStatement(SourceLoc loc, ExprPtr value)
        : Statement(Kind::Print, loc), value(std::move(value)) {}
    void accept(ASTVisitor& visitor) override;

    ExprPtr value;
};

class BlockStatement : public Statement {
public:
    explicit BlockStatement(SourceLoc loc) : Statement(Kind::Block, loc) {}
    void accept(ASTVisitor& visitor) override;

    StmtList statements;
};

class IfStatement : public Statement {
public:
    IfStatement(SourceLoc loc, ExprPtr cond, StmtPtr thenBranch, StmtPtr elseBranch)
        : Statement(Kind::If, loc), condition(std::move(cond)),
          thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}
    void accept(ASTVisitor& visitor) override;

    ExprPtr condition;
    StmtPtr thenBranch;             // always a BlockStatement
    StmtPtr elseBranch;             // BlockStatement, IfStatement (else-if) or null
};

class WhileStatement : public Statement {
public:
    WhileStatement(SourceLoc loc, ExprPtr cond, StmtPtr body)
        : Statement(Kind::While, loc), condition(std::move(cond)), body(std::move(body)) {}
    void accept(ASTVisitor& visitor) override;

    ExprPtr condition;
    StmtPtr body;                   // always a BlockStatement
};

// -------------------------------------------------------------- program ----
class Program : public ASTNode {
public:
    Program() : ASTNode(SourceLoc{1, 1}) {}
    void accept(ASTVisitor& visitor) override;

    StmtList statements;
};

// -------------------------------------------------------------- visitor ----
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    virtual void visit(IntegerLiteral& node) = 0;
    virtual void visit(BooleanLiteral& node) = 0;
    virtual void visit(VariableExpression& node) = 0;
    virtual void visit(BinaryExpression& node) = 0;
    virtual void visit(UnaryExpression& node) = 0;

    virtual void visit(VariableDeclaration& node) = 0;
    virtual void visit(Assignment& node) = 0;
    virtual void visit(PrintStatement& node) = 0;
    virtual void visit(IfStatement& node) = 0;
    virtual void visit(WhileStatement& node) = 0;
    virtual void visit(BlockStatement& node) = 0;
    virtual void visit(Program& node) = 0;
};

// ------------------------------------------------------------- printers ----
// Indented tree view, e.g.
//     Program
//     `-- VarDecl x
//         `-- Binary +
//             |-- Int 10
//             `-- Binary *
void dumpTree(ASTNode& node, std::ostream& out);

// Convert the AST back to MiniLang source text (used to show the program
// before and after optimization).
std::string toSource(Expression& expr);
void printSource(Program& program, std::ostream& out);

} // namespace minilang
