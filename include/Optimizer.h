// =============================================================================
//  Optimizer.h  --  Phase 4: Machine-independent optimization on the AST
// -----------------------------------------------------------------------------
//  Runs after semantic analysis (so every expression has a type and every
//  variable is resolved to a symbol id) and before LLVM IR generation.
//
//  Transformations:
//    1. Constant folding         10 + 20 * 2          =>  50
//    2. Constant propagation     let a = 10; a + 5    =>  15
//    3. Algebraic simplification x + 0 => x,  x * 1 => x,  x * 0 => 0, ...
//    4. Dead code elimination    if (false) { ... }   =>  (removed)
//                                while (false) { ... } => (removed)
//                                if (true) {A} else {B} => A
//
//  All four run together in one forward walk over the program; the walk is
//  repeated until nothing changes (a fixed point).  Every transformation is
//  recorded so that the driver can show what the optimizer did.
// =============================================================================
#pragma once

#include "AST.h"
#include "Diagnostics.h"

#include <cstdint>
#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace minilang {

struct OptimizationRecord {
    std::string pass;          // "constant-folding", ...
    SourceLoc loc;
    std::string description;   // e.g. "20 * 2  =>  40"
};

struct OptimizationStats {
    int constantFolding = 0;
    int constantPropagation = 0;
    int algebraicSimplification = 0;
    int deadCodeElimination = 0;
    int iterations = 0;

    int total() const {
        return constantFolding + constantPropagation + algebraicSimplification + deadCodeElimination;
    }
};

class Optimizer {
public:
    explicit Optimizer(Diagnostics& diagnostics);

    void optimize(Program& program);

    const std::vector<OptimizationRecord>& records() const { return records_; }
    const OptimizationStats& stats() const { return stats_; }
    void printReport(std::ostream& out) const;

private:
    // A compile-time constant value of a variable (bools stored as 0/1).
    struct Constant {
        Type type;
        int32_t value;
    };
    // Constant propagation environment: symbol id -> known constant value.
    using Env = std::map<int, Constant>;

    bool runPass(Program& program);
    void optimizeList(StmtList& list, Env& env);
    StmtPtr optimizeStmt(StmtPtr stmt, Env& env);
    ExprPtr optimizeExpr(ExprPtr expr, Env& env);
    ExprPtr foldBinary(std::unique_ptr<BinaryExpression> node);
    ExprPtr simplifyBinary(std::unique_ptr<BinaryExpression> node);

    static void collectAssigned(Statement& stmt, std::set<int>& out);
    static Env intersect(const Env& a, const Env& b);

    void record(const char* pass, SourceLoc loc, const std::string& description);

    Diagnostics& diag_;
    std::vector<OptimizationRecord> records_;
    OptimizationStats stats_;
    bool changed_ = false;
    std::set<std::pair<int, int>> warned_;   // avoid repeating warnings across passes
};

} // namespace minilang
