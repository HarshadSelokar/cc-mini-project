// =============================================================================
//  LLVMCodeGen.h  --  Phase 5: LLVM IR generation
// -----------------------------------------------------------------------------
//  Translates the (optimized, type-checked) AST into an LLVM module using the
//  LLVM C++ API (IRBuilder).  The whole program becomes the body of
//
//      define i32 @main() { ... ret i32 0 }
//
//  Mapping from MiniLang to LLVM IR:
//      int / bool               i32 / i1
//      let x = e;               %x = alloca i32 (in the entry block) + store
//      x  (read)                load
//      x = e;                   store
//      + - * / %                add, sub, mul, sdiv, srem
//      < > <= >= == !=          icmp slt / sgt / sle / sge / eq / ne
//      && ||                    short-circuit branches + phi
//      -e  !e                   sub 0, e  /  xor e, true
//      print(e)                 call i32 (ptr, ...) @printf(...)
//      if / if-else             if.then / if.else / if.end blocks + br
//      while                    while.cond / while.body / while.end blocks
//
//  The finished module is checked with llvm::verifyModule before it is
//  written, so only valid IR ever leaves the compiler.
// =============================================================================
#pragma once

#include "AST.h"
#include "Diagnostics.h"

#include <map>
#include <memory>
#include <string>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace minilang {

class LLVMCodeGen : public ASTVisitor {
public:
    LLVMCodeGen(Diagnostics& diagnostics, const std::string& moduleName);
    ~LLVMCodeGen() override;

    // Build the module.  Returns false if the IR failed verification.
    bool generate(Program& program);

    std::string irText() const;                       // textual LLVM IR
    bool writeIR(const std::string& path) const;      // write a .ll file

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
    using Builder = llvm::IRBuilder<>;

    llvm::Value* emit(Expression& expr);              // generate an expression
    llvm::Value* emitShortCircuit(BinaryExpression& node);
    llvm::Type* llvmType(Type type);
    llvm::AllocaInst* createEntryAlloca(llvm::Type* type, const std::string& name);
    llvm::BasicBlock* newBlock(const std::string& name);
    void startBlock(llvm::BasicBlock* block);         // append to function, set insert point
    void declareRuntime();

    Diagnostics& diag_;
    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<Builder> builder_;

    llvm::Function* mainFn_ = nullptr;
    llvm::Function* printf_ = nullptr;
    llvm::Constant* fmtInt_ = nullptr;     // "%d\n"
    llvm::Constant* fmtStr_ = nullptr;     // "%s\n"
    llvm::Constant* strTrue_ = nullptr;    // "true"
    llvm::Constant* strFalse_ = nullptr;   // "false"

    std::map<int, llvm::AllocaInst*> variables_;   // symbol id -> stack slot
    llvm::AllocaInst* lastAlloca_ = nullptr;      // most recent slot in the entry block
    llvm::Value* lastValue_ = nullptr;             // result of the last expression
};

} // namespace minilang
