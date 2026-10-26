// =============================================================================
//  Diagnostics.cpp  --  Formatting of compiler error and warning messages
// =============================================================================
#include "Diagnostics.h"

#include <ostream>
#include <sstream>

namespace minilang {

const char* phaseName(Phase phase) {
    switch (phase) {
    case Phase::Lexer:     return "Lexer";
    case Phase::Parser:    return "Parser";
    case Phase::Semantic:  return "Semantic";
    case Phase::Optimizer: return "Optimizer";
    case Phase::CodeGen:   return "CodeGen";
    case Phase::Driver:    return "Driver";
    }
    return "Compiler";
}

Diagnostics::Diagnostics(std::ostream& out) : out_(out) {}

void Diagnostics::setSource(const std::string& source) {
    lines_.clear();
    std::istringstream in(source);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // Windows CRLF
        lines_.push_back(line);
    }
}

void Diagnostics::error(Phase phase, SourceLoc loc, const std::string& message) {
    ++errors_;
    // Garbage input can produce thousands of errors; after the first few
    // they add nothing, so they are counted but not printed.
    if (errors_ <= kMaxPrintedErrors) emit("Error", phase, loc, message);
    else if (errors_ == kMaxPrintedErrors + 1)
        out_ << "Too many errors; further " << phaseName(phase) << " errors are not shown.\n";
}

void Diagnostics::warning(Phase phase, SourceLoc loc, const std::string& message) {
    if (!warningsEnabled_) return;
    ++warnings_;
    emit("Warning", phase, loc, message);
}

void Diagnostics::emit(const char* severity, Phase phase, SourceLoc loc,
                       const std::string& message) {
    out_ << phaseName(phase) << ' ' << severity;
    if (loc.line > 0) {
        out_ << " [Line " << loc.line;
        if (loc.column > 0) out_ << ", Col " << loc.column;
        out_ << ']';
    }
    out_ << ": " << message << '\n';

    // Quote the source line and put a caret under the offending column.
    if (loc.line > 0 && loc.line <= static_cast<int>(lines_.size())) {
        const std::string& text = lines_[loc.line - 1];
        std::string number = std::to_string(loc.line);
        out_ << "    " << number << " | " << text << '\n';
        if (loc.column > 0) {
            std::string pad;
            for (int i = 1; i < loc.column && i <= static_cast<int>(text.size()); ++i)
                pad += (text[i - 1] == '\t') ? '\t' : ' ';
            out_ << "    " << std::string(number.size(), ' ') << " | " << pad << "^\n";
        }
    }
    out_.flush();
}

} // namespace minilang
