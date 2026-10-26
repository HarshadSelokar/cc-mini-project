// =============================================================================
//  AST.cpp  --  AST node helpers and printers
// -----------------------------------------------------------------------------
//  * accept() implementations (double dispatch for the Visitor pattern)
//  * operator / type names
//  * TreePrinter   : renders the AST as an indented ASCII tree  (--ast)
//  * SourcePrinter : renders the AST back to MiniLang source    (--optimized-ast)
// =============================================================================
#include "AST.h"

#include <ostream>
#include <sstream>

namespace minilang {

// ------------------------------------------------------- accept methods ----
void IntegerLiteral::accept(ASTVisitor& v)      { v.visit(*this); }
void BooleanLiteral::accept(ASTVisitor& v)      { v.visit(*this); }
void VariableExpression::accept(ASTVisitor& v)  { v.visit(*this); }
void BinaryExpression::accept(ASTVisitor& v)    { v.visit(*this); }
void UnaryExpression::accept(ASTVisitor& v)     { v.visit(*this); }
void VariableDeclaration::accept(ASTVisitor& v) { v.visit(*this); }
void Assignment::accept(ASTVisitor& v)          { v.visit(*this); }
void PrintStatement::accept(ASTVisitor& v)      { v.visit(*this); }
void IfStatement::accept(ASTVisitor& v)         { v.visit(*this); }
void WhileStatement::accept(ASTVisitor& v)      { v.visit(*this); }
void BlockStatement::accept(ASTVisitor& v)      { v.visit(*this); }
void Program::accept(ASTVisitor& v)             { v.visit(*this); }

void ASTNode::dump(std::ostream& out) { dumpTree(*this, out); }

// ------------------------------------------------------- names / tables ----
const char* typeName(Type type) {
    switch (type) {
    case Type::Int:     return "int";
    case Type::Bool:    return "bool";
    case Type::Error:   return "<error>";
    case Type::Unknown: return "<unknown>";
    }
    return "<unknown>";
}

const char* opSymbol(BinaryOp op) {
    switch (op) {
    case BinaryOp::Add: return "+";
    case BinaryOp::Sub: return "-";
    case BinaryOp::Mul: return "*";
    case BinaryOp::Div: return "/";
    case BinaryOp::Mod: return "%";
    case BinaryOp::Lt:  return "<";
    case BinaryOp::Gt:  return ">";
    case BinaryOp::Le:  return "<=";
    case BinaryOp::Ge:  return ">=";
    case BinaryOp::Eq:  return "==";
    case BinaryOp::Ne:  return "!=";
    case BinaryOp::And: return "&&";
    case BinaryOp::Or:  return "||";
    }
    return "?";
}

const char* opSymbol(UnaryOp op) { return op == UnaryOp::Neg ? "-" : "!"; }

bool isArithmetic(BinaryOp op) {
    return op == BinaryOp::Add || op == BinaryOp::Sub || op == BinaryOp::Mul ||
           op == BinaryOp::Div || op == BinaryOp::Mod;
}
bool isRelational(BinaryOp op) {
    return op == BinaryOp::Lt || op == BinaryOp::Gt || op == BinaryOp::Le || op == BinaryOp::Ge;
}
bool isEquality(BinaryOp op) { return op == BinaryOp::Eq || op == BinaryOp::Ne; }
bool isLogical(BinaryOp op) { return op == BinaryOp::And || op == BinaryOp::Or; }

// ========================================================= tree printer ====
namespace {

// The visitor first builds a small tree of text labels, which is then
// rendered with ASCII connectors.  Keeping the two steps apart keeps the
// connector logic in one place.
struct Label {
    std::string text;
    std::vector<Label> children;
};

class TreePrinter : public ASTVisitor {
public:
    Label result;

    void visit(IntegerLiteral& n) override { result = {"IntegerLiteral " + std::to_string(n.value) + typed(n), {}}; }
    void visit(BooleanLiteral& n) override { result = {std::string("BooleanLiteral ") + (n.value ? "true" : "false") + typed(n), {}}; }
    void visit(VariableExpression& n) override { result = {"Variable '" + n.name + "'" + typed(n), {}}; }

    void visit(BinaryExpression& n) override {
        Label l{"BinaryExpr '" + std::string(opSymbol(n.op)) + "'" + typed(n), {}};
        l.children.push_back(build(*n.lhs));
        l.children.push_back(build(*n.rhs));
        result = std::move(l);
    }
    void visit(UnaryExpression& n) override {
        Label l{"UnaryExpr '" + std::string(opSymbol(n.op)) + "'" + typed(n), {}};
        l.children.push_back(build(*n.operand));
        result = std::move(l);
    }

    void visit(VariableDeclaration& n) override {
        std::string text = "VarDecl '" + n.name + "'";
        if (n.varType != Type::Unknown) text += std::string(" : ") + typeName(n.varType);
        result = {text, {build(*n.init)}};
    }
    void visit(Assignment& n) override { result = {"Assign '" + n.name + "'", {build(*n.value)}}; }
    void visit(PrintStatement& n) override { result = {"Print", {build(*n.value)}}; }

    void visit(IfStatement& n) override {
        Label l{"If", {}};
        l.children.push_back({"Condition", {build(*n.condition)}});
        l.children.push_back({"Then", {build(*n.thenBranch)}});
        if (n.elseBranch) l.children.push_back({"Else", {build(*n.elseBranch)}});
        result = std::move(l);
    }
    void visit(WhileStatement& n) override {
        Label l{"While", {}};
        l.children.push_back({"Condition", {build(*n.condition)}});
        l.children.push_back({"Body", {build(*n.body)}});
        result = std::move(l);
    }
    void visit(BlockStatement& n) override {
        Label l{"Block", {}};
        for (auto& s : n.statements) l.children.push_back(build(*s));
        result = std::move(l);
    }
    void visit(Program& n) override {
        Label l{"Program", {}};
        for (auto& s : n.statements) l.children.push_back(build(*s));
        result = std::move(l);
    }

    Label build(ASTNode& node) {
        node.accept(*this);
        return std::move(result);
    }

private:
    static std::string typed(Expression& e) {
        // Types are filled in by semantic analysis, so a tree printed right
        // after parsing shows none and the optimized tree shows all of them.
        if (e.type == Type::Unknown || e.type == Type::Error) return "";
        return std::string(" : ") + typeName(e.type);
    }
};

void render(const Label& l, const std::string& prefix, bool isLast, bool isRoot, std::ostream& out) {
    if (isRoot) {
        out << l.text << '\n';
    } else {
        out << prefix << (isLast ? "`-- " : "|-- ") << l.text << '\n';
    }
    std::string childPrefix = isRoot ? "" : prefix + (isLast ? "    " : "|   ");
    for (size_t i = 0; i < l.children.size(); ++i)
        render(l.children[i], childPrefix, i + 1 == l.children.size(), false, out);
}

} // namespace

void dumpTree(ASTNode& node, std::ostream& out) {
    TreePrinter printer;
    render(printer.build(node), "", true, true, out);
}

// ======================================================= source printer ====
namespace {

// Binding strength, used to insert the minimum number of parentheses.
int precedence(const Expression& e) {
    if (e.kind == Expression::Kind::Unary) return 7;
    if (e.kind != Expression::Kind::Binary) {
        // A negative constant (produced by folding) prints as "-5", which
        // behaves like a unary expression.
        if (e.kind == Expression::Kind::IntegerLiteral &&
            static_cast<const IntegerLiteral&>(e).value < 0) return 7;
        return 8;
    }
    switch (static_cast<const BinaryExpression&>(e).op) {
    case BinaryOp::Or:  return 1;
    case BinaryOp::And: return 2;
    case BinaryOp::Eq: case BinaryOp::Ne: return 3;
    case BinaryOp::Lt: case BinaryOp::Gt: case BinaryOp::Le: case BinaryOp::Ge: return 4;
    case BinaryOp::Add: case BinaryOp::Sub: return 5;
    default: return 6;  // * / %
    }
}

std::string exprSource(Expression& e);

std::string wrap(Expression& child, bool needParens) {
    std::string s = exprSource(child);
    return needParens ? "(" + s + ")" : s;
}

std::string exprSource(Expression& e) {
    switch (e.kind) {
    case Expression::Kind::IntegerLiteral:
        return std::to_string(static_cast<IntegerLiteral&>(e).value);
    case Expression::Kind::BooleanLiteral:
        return static_cast<BooleanLiteral&>(e).value ? "true" : "false";
    case Expression::Kind::Variable:
        return static_cast<VariableExpression&>(e).name;
    case Expression::Kind::Unary: {
        auto& u = static_cast<UnaryExpression&>(e);
        return std::string(opSymbol(u.op)) + wrap(*u.operand, precedence(*u.operand) < 7);
    }
    case Expression::Kind::Binary: {
        auto& b = static_cast<BinaryExpression&>(e);
        int p = precedence(b);
        bool nonAssoc = (p == 3 || p == 4);   // relational / equality operators
        bool lhsParens = precedence(*b.lhs) < p || (nonAssoc && precedence(*b.lhs) == p);
        bool rhsParens = precedence(*b.rhs) <= p;   // all operators are left-associative
        return wrap(*b.lhs, lhsParens) + " " + opSymbol(b.op) + " " + wrap(*b.rhs, rhsParens);
    }
    }
    return "?";
}

void stmtSource(Statement& s, int indent, std::ostream& out);

void blockBody(BlockStatement& b, int indent, std::ostream& out) {
    out << "{\n";
    for (auto& st : b.statements) stmtSource(*st, indent + 1, out);
    out << std::string(indent * 4, ' ') << "}";
}

void stmtSource(Statement& s, int indent, std::ostream& out) {
    std::string pad(indent * 4, ' ');
    switch (s.kind) {
    case Statement::Kind::VarDecl: {
        auto& d = static_cast<VariableDeclaration&>(s);
        out << pad << "let " << d.name << " = " << exprSource(*d.init) << ";\n";
        break;
    }
    case Statement::Kind::Assign: {
        auto& a = static_cast<Assignment&>(s);
        out << pad << a.name << " = " << exprSource(*a.value) << ";\n";
        break;
    }
    case Statement::Kind::Print:
        out << pad << "print(" << exprSource(*static_cast<PrintStatement&>(s).value) << ");\n";
        break;
    case Statement::Kind::Block:
        out << pad;
        blockBody(static_cast<BlockStatement&>(s), indent, out);
        out << "\n";
        break;
    case Statement::Kind::While: {
        auto& w = static_cast<WhileStatement&>(s);
        out << pad << "while (" << exprSource(*w.condition) << ") ";
        blockBody(static_cast<BlockStatement&>(*w.body), indent, out);
        out << "\n";
        break;
    }
    case Statement::Kind::If: {
        // Print an else-if chain on one level instead of nesting it.
        auto* i = &static_cast<IfStatement&>(s);
        out << pad;
        while (true) {
            out << "if (" << exprSource(*i->condition) << ") ";
            blockBody(static_cast<BlockStatement&>(*i->thenBranch), indent, out);
            if (!i->elseBranch) break;
            out << " else ";
            if (i->elseBranch->kind == Statement::Kind::If) {
                i = static_cast<IfStatement*>(i->elseBranch.get());
                continue;
            }
            blockBody(static_cast<BlockStatement&>(*i->elseBranch), indent, out);
            break;
        }
        out << "\n";
        break;
    }
    }
}

} // namespace

std::string toSource(Expression& expr) { return exprSource(expr); }

void printSource(Program& program, std::ostream& out) {
    for (auto& s : program.statements) stmtSource(*s, 0, out);
}

} // namespace minilang
