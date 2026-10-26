// =============================================================================
//  Optimizer.cpp  --  Phase 4: AST optimizer (see Optimizer.h for overview)
// -----------------------------------------------------------------------------
//  Expressions are rewritten bottom-up: children first, then the node itself.
//  This way "10 + 20 * 2" first becomes "10 + 40" and then "50".
//
//  Constant propagation is a forward data-flow analysis over the AST.  The
//  environment `env` holds the variables whose value is a known constant at
//  the current program point:
//
//    let x = c;  /  x = c;      env[x] = c         (c is a literal)
//    let x = e;  /  x = e;      remove x from env  (e not constant)
//    if (c) A else B            run A and B on copies of env, then keep only
//                               facts that hold on both paths (intersection)
//    while (c) B                every variable assigned in B may change on
//                               each iteration, so it is removed from env
//                               before c and B are analysed
//
//  Integer arithmetic is folded with 32-bit wrap-around, exactly matching the
//  i32 instructions the LLVM backend emits, so optimization never changes
//  program output.  Division by zero and INT_MIN / -1 are never folded.
// =============================================================================
#include "Optimizer.h"

#include <climits>
#include <iomanip>
#include <ostream>

namespace minilang {

namespace {

ExprPtr makeInt(SourceLoc loc, int32_t value) {
    auto lit = std::make_unique<IntegerLiteral>(loc, value);
    lit->type = Type::Int;
    return lit;
}

ExprPtr makeBool(SourceLoc loc, bool value) {
    auto lit = std::make_unique<BooleanLiteral>(loc, value);
    lit->type = Type::Bool;
    return lit;
}

bool isIntLit(const Expression& e, int32_t v) {
    return e.kind == Expression::Kind::IntegerLiteral &&
           static_cast<const IntegerLiteral&>(e).value == v;
}

bool isBoolLit(const Expression& e, bool v) {
    return e.kind == Expression::Kind::BooleanLiteral &&
           static_cast<const BooleanLiteral&>(e).value == v;
}

bool isLiteral(const Expression& e) {
    return e.kind == Expression::Kind::IntegerLiteral || e.kind == Expression::Kind::BooleanLiteral;
}

int32_t literalValue(const Expression& e) {
    if (e.kind == Expression::Kind::IntegerLiteral) return static_cast<const IntegerLiteral&>(e).value;
    return static_cast<const BooleanLiteral&>(e).value ? 1 : 0;
}

bool sameVariable(const Expression& a, const Expression& b) {
    return a.kind == Expression::Kind::Variable && b.kind == Expression::Kind::Variable &&
           static_cast<const VariableExpression&>(a).symbolId ==
               static_cast<const VariableExpression&>(b).symbolId;
}

// 32-bit two's-complement arithmetic (same as LLVM add/sub/mul on i32).
int32_t wrap(int64_t v) { return static_cast<int32_t>(static_cast<uint32_t>(v)); }

bool isEmptyBlock(const Statement& s) {
    return s.kind == Statement::Kind::Block &&
           static_cast<const BlockStatement&>(s).statements.empty();
}

} // namespace

Optimizer::Optimizer(Diagnostics& diagnostics) : diag_(diagnostics) {}

void Optimizer::record(const char* pass, SourceLoc loc, const std::string& description) {
    changed_ = true;
    records_.push_back({pass, loc, description});
    std::string p = pass;
    if (p == "constant-folding") stats_.constantFolding++;
    else if (p == "constant-propagation") stats_.constantPropagation++;
    else if (p == "algebraic-simplification") stats_.algebraicSimplification++;
    else stats_.deadCodeElimination++;
}

void Optimizer::optimize(Program& program) {
    // Iterate to a fixed point (in practice one or two passes suffice).
    const int maxIterations = 10;
    for (int i = 0; i < maxIterations; ++i) {
        stats_.iterations++;
        if (!runPass(program)) break;
    }
}

bool Optimizer::runPass(Program& program) {
    changed_ = false;
    Env env;
    optimizeList(program.statements, env);
    return changed_;
}

// ---------------------------------------------------------- statements -----
void Optimizer::optimizeList(StmtList& list, Env& env) {
    StmtList result;
    for (auto& stmt : list) {
        StmtPtr s = optimizeStmt(std::move(stmt), env);
        if (!s) continue;                       // statement eliminated
        if (s->kind == Statement::Kind::Block) {
            auto& block = static_cast<BlockStatement&>(*s);
            if (block.statements.empty()) continue;   // drop empty { }
            // A nested block that declares nothing does not need its own
            // scope; splice its statements into the enclosing list.
            bool declares = false;
            for (auto& inner : block.statements)
                if (inner->kind == Statement::Kind::VarDecl) declares = true;
            if (!declares) {
                for (auto& inner : block.statements) result.push_back(std::move(inner));
                continue;
            }
        }
        result.push_back(std::move(s));
    }
    list = std::move(result);
}

StmtPtr Optimizer::optimizeStmt(StmtPtr stmt, Env& env) {
    switch (stmt->kind) {
    case Statement::Kind::VarDecl: {
        auto& d = static_cast<VariableDeclaration&>(*stmt);
        d.init = optimizeExpr(std::move(d.init), env);
        if (isLiteral(*d.init)) env[d.symbolId] = {d.varType, literalValue(*d.init)};
        else env.erase(d.symbolId);
        return stmt;
    }
    case Statement::Kind::Assign: {
        auto& a = static_cast<Assignment&>(*stmt);
        a.value = optimizeExpr(std::move(a.value), env);
        if (isLiteral(*a.value)) env[a.symbolId] = {a.value->type, literalValue(*a.value)};
        else env.erase(a.symbolId);
        return stmt;
    }
    case Statement::Kind::Print: {
        auto& p = static_cast<PrintStatement&>(*stmt);
        p.value = optimizeExpr(std::move(p.value), env);
        return stmt;
    }
    case Statement::Kind::Block: {
        auto& b = static_cast<BlockStatement&>(*stmt);
        optimizeList(b.statements, env);
        return stmt;
    }
    case Statement::Kind::If: {
        auto& i = static_cast<IfStatement&>(*stmt);
        std::string condText = toSource(*i.condition);
        i.condition = optimizeExpr(std::move(i.condition), env);

        // Dead code elimination: the condition is known at compile time.
        if (i.condition->kind == Expression::Kind::BooleanLiteral) {
            bool taken = static_cast<BooleanLiteral&>(*i.condition).value;
            if (taken) {
                record("dead-code-elimination", i.loc,
                       "if (" + condText + ") is always true: kept 'then' branch" +
                           (i.elseBranch ? ", removed 'else' branch" : ""));
                return optimizeStmt(std::move(i.thenBranch), env);
            }
            if (i.elseBranch) {
                record("dead-code-elimination", i.loc,
                       "if (" + condText + ") is always false: removed 'then' branch, kept 'else' branch");
                return optimizeStmt(std::move(i.elseBranch), env);
            }
            record("dead-code-elimination", i.loc,
                   "if (" + condText + ") is always false: removed the whole statement");
            return nullptr;
        }

        Env thenEnv = env;
        i.thenBranch = optimizeStmt(std::move(i.thenBranch), thenEnv);
        Env elseEnv = env;
        if (i.elseBranch) {
            i.elseBranch = optimizeStmt(std::move(i.elseBranch), elseEnv);
            if (i.elseBranch && isEmptyBlock(*i.elseBranch)) {
                record("dead-code-elimination", i.loc, "removed empty 'else' branch");
                i.elseBranch.reset();
            }
        }
        env = intersect(thenEnv, elseEnv);

        // The condition has no side effects, so an if with nothing to do
        // can be removed entirely.
        if (isEmptyBlock(*i.thenBranch) && !i.elseBranch) {
            record("dead-code-elimination", i.loc, "removed 'if (" + toSource(*i.condition) +
                                                       ")' with an empty body");
            return nullptr;
        }
        return stmt;
    }
    case Statement::Kind::While: {
        auto& w = static_cast<WhileStatement&>(*stmt);

        // Variables changed inside the loop are not constant in the loop.
        std::set<int> assigned;
        collectAssigned(*w.body, assigned);
        for (int id : assigned) env.erase(id);

        std::string condText = toSource(*w.condition);
        w.condition = optimizeExpr(std::move(w.condition), env);
        if (isBoolLit(*w.condition, false)) {
            record("dead-code-elimination", w.loc,
                   "while (" + condText + ") never executes: removed the loop");
            return nullptr;
        }
        Env bodyEnv = env;
        w.body = optimizeStmt(std::move(w.body), bodyEnv);
        // After the loop, `env` (without the assigned variables) is still
        // valid: it holds only facts about variables the loop never changes.
        return stmt;
    }
    }
    return stmt;
}

void Optimizer::collectAssigned(Statement& stmt, std::set<int>& out) {
    switch (stmt.kind) {
    case Statement::Kind::Assign:
        out.insert(static_cast<Assignment&>(stmt).symbolId);
        break;
    case Statement::Kind::Block:
        for (auto& s : static_cast<BlockStatement&>(stmt).statements) collectAssigned(*s, out);
        break;
    case Statement::Kind::If: {
        auto& i = static_cast<IfStatement&>(stmt);
        collectAssigned(*i.thenBranch, out);
        if (i.elseBranch) collectAssigned(*i.elseBranch, out);
        break;
    }
    case Statement::Kind::While:
        collectAssigned(*static_cast<WhileStatement&>(stmt).body, out);
        break;
    default:
        break;
    }
}

Optimizer::Env Optimizer::intersect(const Env& a, const Env& b) {
    Env result;
    for (const auto& [id, c] : a) {
        auto it = b.find(id);
        if (it != b.end() && it->second.value == c.value && it->second.type == c.type)
            result[id] = c;
    }
    return result;
}

// --------------------------------------------------------- expressions -----
ExprPtr Optimizer::optimizeExpr(ExprPtr expr, Env& env) {
    switch (expr->kind) {
    case Expression::Kind::IntegerLiteral:
    case Expression::Kind::BooleanLiteral:
        return expr;

    case Expression::Kind::Variable: {
        auto& v = static_cast<VariableExpression&>(*expr);
        auto it = env.find(v.symbolId);
        if (it == env.end()) return expr;
        ExprPtr lit = it->second.type == Type::Bool ? makeBool(v.loc, it->second.value != 0)
                                                    : makeInt(v.loc, it->second.value);
        record("constant-propagation", v.loc, v.name + "  =>  " + toSource(*lit));
        return lit;
    }

    case Expression::Kind::Unary: {
        auto& u = static_cast<UnaryExpression&>(*expr);
        u.operand = optimizeExpr(std::move(u.operand), env);
        std::string before = toSource(u);

        if (isLiteral(*u.operand)) {
            // Show "-(12)" rather than "-12" so the log makes the fold visible.
            if (u.op == UnaryOp::Neg) before = "-(" + toSource(*u.operand) + ")";
            ExprPtr lit = u.op == UnaryOp::Neg
                              ? makeInt(u.loc, wrap(-static_cast<int64_t>(literalValue(*u.operand))))
                              : makeBool(u.loc, literalValue(*u.operand) == 0);
            record("constant-folding", u.loc, before + "  =>  " + toSource(*lit));
            return lit;
        }
        // -(-x) => x   and   !(!x) => x
        if (u.operand->kind == Expression::Kind::Unary &&
            static_cast<UnaryExpression&>(*u.operand).op == u.op) {
            ExprPtr inner = std::move(static_cast<UnaryExpression&>(*u.operand).operand);
            record("algebraic-simplification", u.loc, before + "  =>  " + toSource(*inner));
            return inner;
        }
        return expr;
    }

    case Expression::Kind::Binary: {
        auto& b = static_cast<BinaryExpression&>(*expr);
        b.lhs = optimizeExpr(std::move(b.lhs), env);
        b.rhs = optimizeExpr(std::move(b.rhs), env);
        std::unique_ptr<BinaryExpression> node(static_cast<BinaryExpression*>(expr.release()));
        if (isLiteral(*node->lhs) && isLiteral(*node->rhs)) return foldBinary(std::move(node));
        return simplifyBinary(std::move(node));
    }
    }
    return expr;
}

// Constant folding: both operands are literals.
ExprPtr Optimizer::foldBinary(std::unique_ptr<BinaryExpression> node) {
    const int64_t a = literalValue(*node->lhs);
    const int64_t b = literalValue(*node->rhs);
    const SourceLoc loc = node->loc;
    ExprPtr result;

    switch (node->op) {
    case BinaryOp::Add: result = makeInt(loc, wrap(a + b)); break;
    case BinaryOp::Sub: result = makeInt(loc, wrap(a - b)); break;
    case BinaryOp::Mul: result = makeInt(loc, wrap(a * b)); break;
    case BinaryOp::Div:
    case BinaryOp::Mod:
        if (b == 0 || (a == INT32_MIN && b == -1)) {
            // Leave it for run time; folding would hide undefined behaviour.
            if (warned_.insert({loc.line, loc.column}).second)
                diag_.warning(Phase::Optimizer, loc,
                              b == 0 ? "expression '" + toSource(*node) + "' divides by zero at run time"
                                     : "expression '" + toSource(*node) + "' overflows at run time");
            return node;
        }
        result = makeInt(loc, node->op == BinaryOp::Div ? static_cast<int32_t>(a / b)
                                                         : static_cast<int32_t>(a % b));
        break;
    case BinaryOp::Lt:  result = makeBool(loc, a < b);  break;
    case BinaryOp::Gt:  result = makeBool(loc, a > b);  break;
    case BinaryOp::Le:  result = makeBool(loc, a <= b); break;
    case BinaryOp::Ge:  result = makeBool(loc, a >= b); break;
    case BinaryOp::Eq:  result = makeBool(loc, a == b); break;
    case BinaryOp::Ne:  result = makeBool(loc, a != b); break;
    case BinaryOp::And: result = makeBool(loc, a && b); break;
    case BinaryOp::Or:  result = makeBool(loc, a || b); break;
    }
    record("constant-folding", loc, toSource(*node) + "  =>  " + toSource(*result));
    return result;
}

// Algebraic simplification: identities that hold for every value of x.
// MiniLang expressions have no side effects, so dropping an operand is safe.
ExprPtr Optimizer::simplifyBinary(std::unique_ptr<BinaryExpression> node) {
    Expression& l = *node->lhs;
    Expression& r = *node->rhs;
    const SourceLoc loc = node->loc;
    const std::string before = toSource(*node);
    ExprPtr result;

    switch (node->op) {
    case BinaryOp::Add:
        if (isIntLit(r, 0)) result = std::move(node->lhs);                       // x + 0 => x
        else if (isIntLit(l, 0)) result = std::move(node->rhs);                  // 0 + x => x
        break;
    case BinaryOp::Sub:
        if (isIntLit(r, 0)) result = std::move(node->lhs);                       // x - 0 => x
        else if (sameVariable(l, r)) result = makeInt(loc, 0);                   // x - x => 0
        else if (isIntLit(l, 0)) {                                               // 0 - x => -x
            result = std::make_unique<UnaryExpression>(loc, UnaryOp::Neg, std::move(node->rhs));
            result->type = Type::Int;
        }
        break;
    case BinaryOp::Mul:
        if (isIntLit(r, 1)) result = std::move(node->lhs);                       // x * 1 => x
        else if (isIntLit(l, 1)) result = std::move(node->rhs);                  // 1 * x => x
        else if (isIntLit(r, 0) || isIntLit(l, 0)) result = makeInt(loc, 0);     // x * 0 => 0
        break;
    case BinaryOp::Div:
        if (isIntLit(r, 1)) result = std::move(node->lhs);                       // x / 1 => x
        break;
    case BinaryOp::Mod:
        if (isIntLit(r, 1)) result = makeInt(loc, 0);                            // x % 1 => 0
        break;
    case BinaryOp::And:
        if (isBoolLit(r, true)) result = std::move(node->lhs);                   // x && true  => x
        else if (isBoolLit(l, true)) result = std::move(node->rhs);              // true && x  => x
        else if (isBoolLit(r, false) || isBoolLit(l, false)) result = makeBool(loc, false);
        else if (sameVariable(l, r)) result = std::move(node->lhs);              // x && x => x
        break;
    case BinaryOp::Or:
        if (isBoolLit(r, false)) result = std::move(node->lhs);                  // x || false => x
        else if (isBoolLit(l, false)) result = std::move(node->rhs);             // false || x => x
        else if (isBoolLit(r, true) || isBoolLit(l, true)) result = makeBool(loc, true);
        else if (sameVariable(l, r)) result = std::move(node->lhs);              // x || x => x
        break;
    case BinaryOp::Eq: case BinaryOp::Le: case BinaryOp::Ge:
        if (sameVariable(l, r)) result = makeBool(loc, true);                    // x == x => true
        break;
    case BinaryOp::Ne: case BinaryOp::Lt: case BinaryOp::Gt:
        if (sameVariable(l, r)) result = makeBool(loc, false);                   // x < x => false
        break;
    }

    if (!result) return node;
    record("algebraic-simplification", loc, before + "  =>  " + toSource(*result));
    return result;
}

// -------------------------------------------------------------- report -----
void Optimizer::printReport(std::ostream& out) const {
    if (records_.empty()) {
        out << "No optimizations applied.\n";
    } else {
        for (const auto& r : records_) {
            out << "  [" << std::left << std::setw(24) << r.pass << std::right << "] line "
                << std::setw(3) << r.loc.line << ":  " << r.description << '\n';
        }
    }
    out << "\nSummary: " << stats_.constantFolding << " constant folding, "
        << stats_.constantPropagation << " constant propagation, "
        << stats_.algebraicSimplification << " algebraic simplification, "
        << stats_.deadCodeElimination << " dead code elimination  ("
        << stats_.iterations << (stats_.iterations == 1 ? " pass" : " passes") << ")\n";
}

} // namespace minilang
