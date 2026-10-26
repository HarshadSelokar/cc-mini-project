// =============================================================================
//  LLVMCodeGen.cpp  --  Phase 5: LLVM IR generation (see header for mapping)
// =============================================================================
#include "LLVMCodeGen.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/TargetParser/Triple.h>

namespace minilang {

LLVMCodeGen::LLVMCodeGen(Diagnostics& diagnostics, const std::string& moduleName)
    : diag_(diagnostics),
      context_(std::make_unique<llvm::LLVMContext>()),
      module_(std::make_unique<llvm::Module>(moduleName, *context_)),
      builder_(std::make_unique<Builder>(*context_)) {
    // Target the machine we are running on, so clang can link the module
    // without "overriding the module target triple" warnings.
    module_->setTargetTriple(llvm::Triple(llvm::sys::getDefaultTargetTriple()));
}

LLVMCodeGen::~LLVMCodeGen() = default;

llvm::Type* LLVMCodeGen::llvmType(Type type) {
    return type == Type::Bool ? builder_->getInt1Ty() : builder_->getInt32Ty();
}

// --------------------------------------------------------------- driver ----
bool LLVMCodeGen::generate(Program& program) {
    // int main(void)
    auto* mainType = llvm::FunctionType::get(builder_->getInt32Ty(), /*isVarArg=*/false);
    mainFn_ = llvm::Function::Create(mainType, llvm::Function::ExternalLinkage, "main", *module_);
    startBlock(newBlock("entry"));

    declareRuntime();
    program.accept(*this);

    builder_->CreateRet(builder_->getInt32(0));

    // Verify the generated IR (types, terminators, dominance, ...).
    std::string problems;
    llvm::raw_string_ostream os(problems);
    if (llvm::verifyModule(*module_, &os)) {
        os.flush();
        diag_.error(Phase::CodeGen, SourceLoc{}, "generated LLVM IR failed verification:\n" + problems);
        return false;
    }
    return true;
}

std::string LLVMCodeGen::irText() const {
    std::string text;
    llvm::raw_string_ostream os(text);
    module_->print(os, nullptr);
    os.flush();
    return text;
}

bool LLVMCodeGen::writeIR(const std::string& path) const {
    std::error_code ec;
    llvm::raw_fd_ostream out(path, ec, llvm::sys::fs::OF_Text);
    if (ec) {
        diag_.error(Phase::CodeGen, SourceLoc{}, "cannot write '" + path + "': " + ec.message());
        return false;
    }
    module_->print(out, nullptr);
    return true;
}

// ------------------------------------------------------- runtime support ----
// print() is implemented with the C library's printf:
//     declare i32 @printf(ptr, ...)
// plus constant format strings "%d\n", "%s\n", "true", "false".
void LLVMCodeGen::declareRuntime() {
    auto* printfType = llvm::FunctionType::get(builder_->getInt32Ty(), {builder_->getPtrTy()},
                                               /*isVarArg=*/true);
    printf_ = llvm::Function::Create(printfType, llvm::Function::ExternalLinkage, "printf", *module_);

    fmtInt_ = builder_->CreateGlobalString("%d\n", ".fmt.int", 0, module_.get());
    fmtStr_ = builder_->CreateGlobalString("%s\n", ".fmt.str", 0, module_.get());
    strTrue_ = builder_->CreateGlobalString("true", ".str.true", 0, module_.get());
    strFalse_ = builder_->CreateGlobalString("false", ".str.false", 0, module_.get());
}

// -------------------------------------------------------- block helpers ----
llvm::BasicBlock* LLVMCodeGen::newBlock(const std::string& name) {
    // Created detached; startBlock() appends it, so blocks appear in the
    // .ll file in the order they are filled (easier to read).
    return llvm::BasicBlock::Create(*context_, name);
}

void LLVMCodeGen::startBlock(llvm::BasicBlock* block) {
    mainFn_->insert(mainFn_->end(), block);
    builder_->SetInsertPoint(block);
}

// All stack slots go into the entry block (the standard pattern that LLVM's
// mem2reg pass can later promote to SSA registers).
llvm::AllocaInst* LLVMCodeGen::createEntryAlloca(llvm::Type* type, const std::string& name) {
    llvm::BasicBlock& entry = mainFn_->getEntryBlock();
    // Insert after the previous alloca so slots appear in declaration order.
    Builder tmp(&entry, lastAlloca_ ? std::next(lastAlloca_->getIterator()) : entry.begin());
    lastAlloca_ = tmp.CreateAlloca(type, nullptr, name);
    return lastAlloca_;
}

// ---------------------------------------------------------- expressions ----
llvm::Value* LLVMCodeGen::emit(Expression& expr) {
    lastValue_ = nullptr;
    expr.accept(*this);
    return lastValue_;
}

void LLVMCodeGen::visit(IntegerLiteral& node) { lastValue_ = builder_->getInt32(static_cast<uint32_t>(node.value)); }

void LLVMCodeGen::visit(BooleanLiteral& node) { lastValue_ = builder_->getInt1(node.value); }

void LLVMCodeGen::visit(VariableExpression& node) {
    llvm::AllocaInst* slot = variables_.at(node.symbolId);
    lastValue_ = builder_->CreateLoad(slot->getAllocatedType(), slot, node.name + ".val");
}

void LLVMCodeGen::visit(BinaryExpression& node) {
    if (isLogical(node.op)) {
        lastValue_ = emitShortCircuit(node);
        return;
    }
    llvm::Value* l = emit(*node.lhs);
    llvm::Value* r = emit(*node.rhs);
    auto& b = *builder_;
    switch (node.op) {
    case BinaryOp::Add: lastValue_ = b.CreateAdd(l, r, "add"); break;
    case BinaryOp::Sub: lastValue_ = b.CreateSub(l, r, "sub"); break;
    case BinaryOp::Mul: lastValue_ = b.CreateMul(l, r, "mul"); break;
    case BinaryOp::Div: lastValue_ = b.CreateSDiv(l, r, "div"); break;
    case BinaryOp::Mod: lastValue_ = b.CreateSRem(l, r, "rem"); break;
    case BinaryOp::Lt:  lastValue_ = b.CreateICmpSLT(l, r, "lt"); break;
    case BinaryOp::Gt:  lastValue_ = b.CreateICmpSGT(l, r, "gt"); break;
    case BinaryOp::Le:  lastValue_ = b.CreateICmpSLE(l, r, "le"); break;
    case BinaryOp::Ge:  lastValue_ = b.CreateICmpSGE(l, r, "ge"); break;
    case BinaryOp::Eq:  lastValue_ = b.CreateICmpEQ(l, r, "eq"); break;
    case BinaryOp::Ne:  lastValue_ = b.CreateICmpNE(l, r, "ne"); break;
    case BinaryOp::And:
    case BinaryOp::Or:  break;   // handled above
    }
}

// a && b  :  if a is false the result is false and b is never evaluated.
// a || b  :  if a is true  the result is true  and b is never evaluated.
//
//       entry:  %a = ...            br i1 %a, label %and.rhs, label %and.end
//     and.rhs:  %b = ...            br label %and.end
//     and.end:  %r = phi i1 [ false, %entry ], [ %b, %and.rhs ]
llvm::Value* LLVMCodeGen::emitShortCircuit(BinaryExpression& node) {
    const bool isAnd = node.op == BinaryOp::And;
    const std::string prefix = isAnd ? "and" : "or";

    llvm::Value* l = emit(*node.lhs);
    llvm::BasicBlock* lhsBlock = builder_->GetInsertBlock();
    llvm::BasicBlock* rhsBlock = newBlock(prefix + ".rhs");
    llvm::BasicBlock* endBlock = newBlock(prefix + ".end");

    if (isAnd) builder_->CreateCondBr(l, rhsBlock, endBlock);
    else       builder_->CreateCondBr(l, endBlock, rhsBlock);

    startBlock(rhsBlock);
    llvm::Value* r = emit(*node.rhs);
    llvm::BasicBlock* rhsEnd = builder_->GetInsertBlock();   // rhs may have added blocks
    builder_->CreateBr(endBlock);

    startBlock(endBlock);
    llvm::PHINode* phi = builder_->CreatePHI(builder_->getInt1Ty(), 2, prefix);
    phi->addIncoming(builder_->getInt1(!isAnd), lhsBlock);
    phi->addIncoming(r, rhsEnd);
    return phi;
}

void LLVMCodeGen::visit(UnaryExpression& node) {
    llvm::Value* v = emit(*node.operand);
    lastValue_ = node.op == UnaryOp::Neg ? builder_->CreateNeg(v, "neg") : builder_->CreateNot(v, "not");
}

// ----------------------------------------------------------- statements ----
void LLVMCodeGen::visit(VariableDeclaration& node) {
    llvm::Value* init = emit(*node.init);
    llvm::AllocaInst* slot = createEntryAlloca(llvmType(node.varType), node.name);
    variables_[node.symbolId] = slot;
    builder_->CreateStore(init, slot);
}

void LLVMCodeGen::visit(Assignment& node) {
    llvm::Value* value = emit(*node.value);
    builder_->CreateStore(value, variables_.at(node.symbolId));
}

void LLVMCodeGen::visit(PrintStatement& node) {
    llvm::Value* value = emit(*node.value);
    if (node.value->type == Type::Bool) {
        // printf("%s\n", value ? "true" : "false")
        llvm::Value* text = builder_->CreateSelect(value, strTrue_, strFalse_, "bool.str");
        builder_->CreateCall(printf_, {fmtStr_, text});
    } else {
        builder_->CreateCall(printf_, {fmtInt_, value});
    }
}

//            br i1 %cond, label %if.then, label %if.else
//  if.then:  ...                    br label %if.end
//  if.else:  ...                    br label %if.end
//  if.end:
void LLVMCodeGen::visit(IfStatement& node) {
    llvm::Value* cond = emit(*node.condition);

    llvm::BasicBlock* thenBlock = newBlock("if.then");
    llvm::BasicBlock* elseBlock = node.elseBranch ? newBlock("if.else") : nullptr;
    llvm::BasicBlock* endBlock = newBlock("if.end");

    builder_->CreateCondBr(cond, thenBlock, elseBlock ? elseBlock : endBlock);

    startBlock(thenBlock);
    node.thenBranch->accept(*this);
    builder_->CreateBr(endBlock);

    if (elseBlock) {
        startBlock(elseBlock);
        node.elseBranch->accept(*this);
        builder_->CreateBr(endBlock);
    }
    startBlock(endBlock);
}

//              br label %while.cond
//  while.cond: %c = ...             br i1 %c, label %while.body, label %while.end
//  while.body: ...                  br label %while.cond
//  while.end:
void LLVMCodeGen::visit(WhileStatement& node) {
    llvm::BasicBlock* condBlock = newBlock("while.cond");
    llvm::BasicBlock* bodyBlock = newBlock("while.body");
    llvm::BasicBlock* endBlock = newBlock("while.end");

    builder_->CreateBr(condBlock);

    startBlock(condBlock);
    llvm::Value* cond = emit(*node.condition);
    builder_->CreateCondBr(cond, bodyBlock, endBlock);

    startBlock(bodyBlock);
    node.body->accept(*this);
    builder_->CreateBr(condBlock);

    startBlock(endBlock);
}

void LLVMCodeGen::visit(BlockStatement& node) {
    for (auto& s : node.statements) s->accept(*this);
}

void LLVMCodeGen::visit(Program& node) {
    for (auto& s : node.statements) s->accept(*this);
}

} // namespace minilang
