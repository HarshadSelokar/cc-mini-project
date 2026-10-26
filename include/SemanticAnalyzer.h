// =============================================================================
//  SemanticAnalyzer.h  --  Phase 3: Semantic analysis
// -----------------------------------------------------------------------------
//  Walks the AST after parsing and checks the rules a context-free grammar
//  cannot express:
//
//    * every variable is declared before use                (scope rules)
//    * no variable is declared twice in the same scope
//    * operand types fit the operator     (int + int, bool && bool, ...)
//    * assignments keep the variable's type (a variable's type is fixed by
//      the initializer of its 'let' declaration)
//    * if / while conditions are bool
//    * no division or modulo by the constant 0
//
//  While checking, it annotates the AST: each expression gets its type and
//  each variable reference gets the id of the declaration it refers to.
//  The driver does not run the optimizer or the LLVM backend if any
//  semantic error was reported.
// =============================================================================
#pragma once

#include "AST.h"
#include "Diagnostics.h"
#include "SymbolTable.h"

#include <set>
#include <string>

namespace minilang {

class SemanticAnalyzer : public ASTVisitor {
public:
    explicit SemanticAnalyzer(Diagnostics& diagnostics);

    // Returns true if the program is semantically valid.
    bool analyze(Program& program);

    const SymbolTable& symbols() const { return symbols_; }

    void visit(IntegerLiteral& node) override;
    void visit(BooleanLiteral& node) override;
    void visit(VariableExpression& node) override;
    void visit(BinaryExpression& node) override;
    void visit(UnaryExpression& node) override;

    void visit(VariableDeclaration& node) override;
    void visit(Assignment& node) override;
    void visit(PrintStatement& node) override;
    void visit(IfStatement& node) override;
    void visit(WhileStatement& node) override;
    void visit(BlockStatement& node) override;
    void visit(Program& node) override;

private:
    Type check(Expression& expr);   // visit an expression and return its type
    void checkCondition(Expression& cond, const char* construct);
    void error(SourceLoc loc, const std::string& message);
    void undeclared(SourceLoc loc, const std::string& name, const std::string& hint = "");

    Diagnostics& diag_;
    SymbolTable symbols_;
    std::set<std::string> reportedUndeclared_;   // report each unknown name once
};

} // namespace minilang
