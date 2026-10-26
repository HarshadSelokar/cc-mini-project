// =============================================================================
//  SymbolTable.h  --  Scoped symbol table used by semantic analysis
// -----------------------------------------------------------------------------
//  MiniLang has block scoping: every { ... } opens a new scope.  The table is
//  a stack of scopes; each scope maps a variable name to a symbol id.
//
//      let x = 1;          // scope 0: x -> #0
//      {
//          let x = true;   // scope 1: x -> #1   (shadows #0)
//          print(x);       // resolves to #1
//      }
//      print(x);           // resolves to #0 again
//
//  Every declaration gets a unique id, which is stored in the AST so that the
//  optimizer and the code generator never confuse shadowed variables.
// =============================================================================
#pragma once

#include "AST.h"

#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace minilang {

struct Symbol {
    int id = -1;
    std::string name;
    Type type = Type::Unknown;
    int scopeDepth = 0;         // 0 = global scope
    SourceLoc declLoc;
    int reads = 0;              // number of times the value is read
    int writes = 0;             // number of assignments after the declaration
};

class SymbolTable {
public:
    SymbolTable();

    void enterScope();
    void exitScope();
    int depth() const { return static_cast<int>(scopes_.size()) - 1; }

    // Add a symbol to the innermost scope.  Returns nullptr if the name is
    // already declared in that scope (duplicate declaration).
    Symbol* declare(const std::string& name, Type type, SourceLoc loc);

    // Search from the innermost scope outwards.
    Symbol* lookup(const std::string& name);
    // Search only the innermost scope.
    Symbol* lookupCurrentScope(const std::string& name);

    Symbol& get(int id) { return symbols_[id]; }
    const std::vector<Symbol>& all() const { return symbols_; }

    void print(std::ostream& out) const;

private:
    std::vector<std::unordered_map<std::string, int>> scopes_;
    std::vector<Symbol> symbols_;   // every symbol ever declared, indexed by id
};

} // namespace minilang
