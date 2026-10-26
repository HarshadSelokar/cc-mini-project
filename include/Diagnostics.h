// =============================================================================
//  Diagnostics.h  --  Error reporting shared by every compiler phase
// -----------------------------------------------------------------------------
//  Every phase (lexer, parser, semantic analyzer, optimizer, code generator)
//  reports problems through one Diagnostics object.  Messages are formatted as
//
//      Semantic Error [Line 3, Col 1]: variable 'count' is not declared
//          3 | count = count + 1;
//            | ^
//
//  The driver checks hasErrors() after each phase and stops the pipeline
//  before the next phase runs, so later phases never see invalid input.
// =============================================================================
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

namespace minilang {

// A position in the source file (1-based).  line == 0 means "unknown".
struct SourceLoc {
    int line = 0;
    int column = 0;
};

enum class Phase { Lexer, Parser, Semantic, Optimizer, CodeGen, Driver };

const char* phaseName(Phase phase);

class Diagnostics {
public:
    explicit Diagnostics(std::ostream& out);

    // Remember the program text so that messages can quote the offending line.
    void setSource(const std::string& source);

    void error(Phase phase, SourceLoc loc, const std::string& message);
    void warning(Phase phase, SourceLoc loc, const std::string& message);

    bool hasErrors() const { return errors_ > 0; }
    int errorCount() const { return errors_; }
    int warningCount() const { return warnings_; }

    // Warnings can be silenced (used when the same AST is printed twice).
    void setWarningsEnabled(bool enabled) { warningsEnabled_ = enabled; }

private:
    static constexpr int kMaxPrintedErrors = 20;

    void emit(const char* severity, Phase phase, SourceLoc loc, const std::string& message);

    std::ostream& out_;
    std::vector<std::string> lines_;
    int errors_ = 0;
    int warnings_ = 0;
    bool warningsEnabled_ = true;
};

} // namespace minilang
