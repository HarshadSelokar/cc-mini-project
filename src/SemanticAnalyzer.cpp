// =============================================================================
//  SemanticAnalyzer.cpp  --  Phase 3: Semantic analysis (see header for rules)
// -----------------------------------------------------------------------------
//  Typing rules (T = int or bool):
//
//      int literal                 : int
//      true / false                : bool
//      x                           : type of x's declaration
//      int  (+ - * / %)  int       : int
//      int  (< > <= >=)  int       : bool
//      T    (== !=)      T         : bool
//      bool (&& ||)      bool      : bool
//      - int                       : int
//      ! bool                      : bool
//
//  Error recovery: a sub-expression with an error gets Type::Error; operators
//  with an Error operand do not report again, so one mistake produces one
//  message instead of a cascade.
// =============================================================================
#include "SemanticAnalyzer.h"

namespace minilang {

SemanticAnalyzer::SemanticAnalyzer(Diagnostics& diagnostics) : diag_(diagnostics) {}

bool SemanticAnalyzer::analyze(Program& program) {
    int before = diag_.errorCount();
    program.accept(*this);

    // Warn about variables that are declared but never read.
    for (const Symbol& s : symbols_.all())
        if (s.reads == 0)
            diag_.warning(Phase::Semantic, s.declLoc,
                          "variable '" + s.name + "' is declared but never used");

    return diag_.errorCount() == before;
}

void SemanticAnalyzer::error(SourceLoc loc, const std::string& message) {
    diag_.error(Phase::Semantic, loc, message);
}

void SemanticAnalyzer::undeclared(SourceLoc loc, const std::string& name, const std::string& hint) {
    // One message per unknown name; later uses would only repeat it.
    if (!reportedUndeclared_.insert(name).second) return;
    error(loc, "variable '" + name + "' is not declared" + hint);
}

Type SemanticAnalyzer::check(Expression& expr) {
    expr.accept(*this);
    return expr.type;
}

void SemanticAnalyzer::checkCondition(Expression& cond, const char* construct) {
    Type t = check(cond);
    if (t != Type::Bool && t != Type::Error)
        error(cond.loc, std::string("condition of '") + construct + "' must be of type 'bool', but has type '" +
                            typeName(t) + "' (hint: compare it, e.g. 'x != 0')");
}

// ---------------------------------------------------------- expressions ----
void SemanticAnalyzer::visit(IntegerLiteral& node) { node.type = Type::Int; }

void SemanticAnalyzer::visit(BooleanLiteral& node) { node.type = Type::Bool; }

void SemanticAnalyzer::visit(VariableExpression& node) {
    Symbol* sym = symbols_.lookup(node.name);
    if (!sym) {
        undeclared(node.loc, node.name);
        node.type = Type::Error;
        return;
    }
    sym->reads++;
    node.symbolId = sym->id;
    node.type = sym->type;
}

void SemanticAnalyzer::visit(BinaryExpression& node) {
    Type l = check(*node.lhs);
    Type r = check(*node.rhs);
    const std::string op = opSymbol(node.op);

    if (l == Type::Error || r == Type::Error) {
        node.type = Type::Error;
        return;
    }

    auto mismatch = [&](const char* expected) {
        error(node.loc, "invalid operands to '" + op + "': expected " + expected + ", but got '" +
                            typeName(l) + "' and '" + typeName(r) + "'");
        node.type = Type::Error;
    };

    if (isArithmetic(node.op)) {
        if (l != Type::Int || r != Type::Int) return mismatch("'int' and 'int'");
        if ((node.op == BinaryOp::Div || node.op == BinaryOp::Mod) &&
            node.rhs->kind == Expression::Kind::IntegerLiteral &&
            static_cast<IntegerLiteral&>(*node.rhs).value == 0) {
            error(node.rhs->loc, std::string(node.op == BinaryOp::Div ? "division" : "modulo") +
                                     " by zero");
            node.type = Type::Error;
            return;
        }
        node.type = Type::Int;
    } else if (isRelational(node.op)) {
        if (l != Type::Int || r != Type::Int) return mismatch("'int' and 'int'");
        node.type = Type::Bool;
    } else if (isEquality(node.op)) {
        if (l != r) {
            error(node.loc, "type mismatch: cannot compare '" + std::string(typeName(l)) + "' with '" +
                                typeName(r) + "' using '" + op + "'");
            node.type = Type::Error;
            return;
        }
        node.type = Type::Bool;
    } else {   // && ||
        if (l != Type::Bool || r != Type::Bool) return mismatch("'bool' and 'bool'");
        node.type = Type::Bool;
    }
}

void SemanticAnalyzer::visit(UnaryExpression& node) {
    Type t = check(*node.operand);
    if (t == Type::Error) {
        node.type = Type::Error;
        return;
    }
    Type expected = node.op == UnaryOp::Neg ? Type::Int : Type::Bool;
    if (t != expected) {
        error(node.loc, std::string("invalid operand to unary '") + opSymbol(node.op) + "': expected '" +
                            typeName(expected) + "', but got '" + typeName(t) + "'");
        node.type = Type::Error;
        return;
    }
    node.type = expected;
}

// ----------------------------------------------------------- statements ----
void SemanticAnalyzer::visit(VariableDeclaration& node) {
    // The initializer is checked first: in "let x = x + 1;" the x on the
    // right refers to an outer x (or is an error), never to the new one.
    Type t = check(*node.init);

    if (Symbol* previous = symbols_.lookupCurrentScope(node.name)) {
        error(node.loc, "duplicate declaration: variable '" + node.name +
                            "' is already declared in this scope (at line " +
                            std::to_string(previous->declLoc.line) + ")");
        return;
    }
    // Declare even when the initializer has an error, so later uses of the
    // name do not produce extra "not declared" messages.
    Symbol* sym = symbols_.declare(node.name, t, node.loc);
    node.varType = t;
    node.symbolId = sym->id;
}

void SemanticAnalyzer::visit(Assignment& node) {
    // Check the target first so "x = x + 1;" with undeclared x reports the
    // assignment (the more helpful message).
    Symbol* sym = symbols_.lookup(node.name);
    if (!sym) {
        undeclared(node.loc, node.name, " (use 'let " + node.name + " = ...;' to declare it)");
        check(*node.value);
        return;
    }
    Type t = check(*node.value);
    sym->writes++;
    node.symbolId = sym->id;
    if (t != Type::Error && sym->type != Type::Error && t != sym->type)
        error(node.value->loc, "invalid assignment: cannot assign a value of type '" +
                                   std::string(typeName(t)) + "' to variable '" + node.name +
                                   "' of type '" + typeName(sym->type) + "'");
}

void SemanticAnalyzer::visit(PrintStatement& node) { check(*node.value); }

void SemanticAnalyzer::visit(IfStatement& node) {
    checkCondition(*node.condition, "if");
    node.thenBranch->accept(*this);
    if (node.elseBranch) node.elseBranch->accept(*this);
}

void SemanticAnalyzer::visit(WhileStatement& node) {
    checkCondition(*node.condition, "while");
    node.body->accept(*this);
}

void SemanticAnalyzer::visit(BlockStatement& node) {
    symbols_.enterScope();
    for (auto& s : node.statements) s->accept(*this);
    symbols_.exitScope();
}

void SemanticAnalyzer::visit(Program& node) {
    for (auto& s : node.statements) s->accept(*this);
}

} // namespace minilang
