// =============================================================================
//  SymbolTable.cpp  --  Scoped symbol table (see SymbolTable.h)
// =============================================================================
#include "SymbolTable.h"

#include <iomanip>
#include <ostream>

namespace minilang {

SymbolTable::SymbolTable() { enterScope(); }   // the global scope

void SymbolTable::enterScope() { scopes_.emplace_back(); }

void SymbolTable::exitScope() {
    if (scopes_.size() > 1) scopes_.pop_back();
}

Symbol* SymbolTable::declare(const std::string& name, Type type, SourceLoc loc) {
    auto& scope = scopes_.back();
    if (scope.count(name)) return nullptr;

    Symbol sym;
    sym.id = static_cast<int>(symbols_.size());
    sym.name = name;
    sym.type = type;
    sym.scopeDepth = depth();
    sym.declLoc = loc;
    symbols_.push_back(sym);
    scope[name] = sym.id;
    return &symbols_.back();
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &symbols_[found->second];
    }
    return nullptr;
}

Symbol* SymbolTable::lookupCurrentScope(const std::string& name) {
    auto found = scopes_.back().find(name);
    return found == scopes_.back().end() ? nullptr : &symbols_[found->second];
}

void SymbolTable::print(std::ostream& out) const {
    out << std::left
        << std::setw(5) << "ID" << std::setw(14) << "NAME" << std::setw(7) << "TYPE"
        << std::setw(7) << "SCOPE" << std::setw(10) << "DECLARED"
        << std::setw(7) << "READS" << "WRITES\n";
    out << std::string(56, '-') << '\n';
    for (const Symbol& s : symbols_) {
        out << std::setw(5) << ("#" + std::to_string(s.id)) << std::setw(14) << s.name
            << std::setw(7) << typeName(s.type) << std::setw(7) << s.scopeDepth
            << std::setw(10) << ("line " + std::to_string(s.declLoc.line))
            << std::setw(7) << s.reads << s.writes << '\n';
    }
    if (symbols_.empty()) out << "(no variables)\n";
    out << std::right;
}

} // namespace minilang
