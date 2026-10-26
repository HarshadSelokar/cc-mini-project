// =============================================================================
//  main.cpp  --  MiniLang compiler driver (command-line interface)
// -----------------------------------------------------------------------------
//  Runs the compiler phases in order and stops at the first phase that
//  reports an error:
//
//      source --[1 Lexer]--> tokens --[2 Parser]--> AST
//             --[3 Semantic analysis]--> annotated AST
//             --[4 Optimizer]--> optimized AST
//             --[5 LLVM code generation]--> LLVM IR (output.ll)
//             --[6 clang]--> native executable --> run
//
//  Usage:  minilang <file.ml> [options]      (see printUsage below)
// =============================================================================
#include "AST.h"
#include "Diagnostics.h"
#include "LLVMCodeGen.h"
#include "Lexer.h"
#include "Optimizer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MINILANG_LLVM_BINDIR
#define MINILANG_LLVM_BINDIR ""
#endif
#ifndef MINILANG_VERSION
#define MINILANG_VERSION "1.0"
#endif

using namespace minilang;

namespace {

struct Options {
    std::string input;
    std::string output = "output.ll";
    bool tokens = false;
    bool ast = false;
    bool semantic = false;
    bool optimizedAst = false;
    bool llvm = false;
    bool run = false;
    bool demo = false;
    bool optimize = true;
};

void printUsage(std::ostream& out) {
    out << "MiniLang Compiler " << MINILANG_VERSION << " (LLVM backend)\n\n"
        << "Usage: minilang <file.ml> [options]\n\n"
        << "Options:\n"
        << "  --tokens          print the token stream (lexical analysis)\n"
        << "  --ast             print the abstract syntax tree (syntax analysis)\n"
        << "  --semantic        print the symbol table (semantic analysis)\n"
        << "  --optimized-ast   print optimizations applied and the optimized AST\n"
        << "  --llvm            write LLVM IR to the output file and print it\n"
        << "  --run             compile to a native executable with clang and run it\n"
        << "  --demo            show every compiler stage step by step, then run\n"
        << "  -o <file>         LLVM IR output file (default: output.ll)\n"
        << "  -O0, --no-opt     disable the AST optimizer\n"
        << "  -h, --help        show this help\n\n"
        << "With no options the program is compiled to output.ll.\n";
}

bool parseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tokens") opt.tokens = true;
        else if (a == "--ast") opt.ast = true;
        else if (a == "--semantic") opt.semantic = true;
        else if (a == "--optimized-ast") opt.optimizedAst = true;
        else if (a == "--llvm") opt.llvm = true;
        else if (a == "--run") opt.run = true;
        else if (a == "--demo") opt.demo = true;
        else if (a == "-O0" || a == "--no-opt") opt.optimize = false;
        else if (a == "-o") {
            if (i + 1 >= argc) {
                std::cerr << "Driver Error: '-o' needs a file name\n";
                return false;
            }
            opt.output = argv[++i];
        } else if (a == "-h" || a == "--help") {
            printUsage(std::cout);
            std::exit(0);
        } else if (!a.empty() && a[0] == '-') {
            std::cerr << "Driver Error: unknown option '" << a << "'\n\n";
            printUsage(std::cerr);
            return false;
        } else if (opt.input.empty()) {
            opt.input = a;
        } else {
            std::cerr << "Driver Error: more than one input file given ('" << opt.input
                      << "' and '" << a << "')\n";
            return false;
        }
    }
    if (opt.input.empty()) {
        printUsage(std::cerr);
        return false;
    }
    return true;
}

bool readFile(const std::string& path, std::string& contents) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    contents = ss.str();
    return true;
}

void banner(const std::string& title) {
    std::cout << "\n" << std::string(70, '=') << "\n  " << title << "\n"
              << std::string(70, '=') << "\n";
}

void printNumberedSource(const std::string& source) {
    std::istringstream in(source);
    std::string line;
    int n = 1;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::printf("%4d | %s\n", n++, line.c_str());
    }
    std::fflush(stdout);
}

// Compile the .ll file with clang and run the executable.  Falls back to the
// LLVM interpreter (lli) if clang is unavailable.  Returns the exit status of
// the MiniLang program, or -1 if it could not be built or started.
int buildAndRun(const std::string& llPath, Diagnostics& diag) {
    std::string err;
    std::vector<llvm::StringRef> searchDirs;
    std::string bindir = MINILANG_LLVM_BINDIR;
    if (!bindir.empty()) searchDirs.push_back(bindir);

    auto find = [&](const char* name) -> std::string {
        if (!searchDirs.empty())
            if (auto p = llvm::sys::findProgramByName(name, searchDirs)) return *p;
        if (auto p = llvm::sys::findProgramByName(name)) return *p;
        return "";
    };

    std::cout.flush();
    std::fflush(stdout);

    llvm::SmallString<256> absLl(llPath);
    llvm::sys::fs::make_absolute(absLl);

    std::string clang = find("clang");
    if (!clang.empty()) {
        llvm::SmallString<256> exe(absLl);
#ifdef _WIN32
        llvm::sys::path::replace_extension(exe, ".exe");
#else
        llvm::sys::path::replace_extension(exe, "");
#endif
        std::string exePath(exe.str());
        std::vector<llvm::StringRef> args = {clang, absLl.str(), "-o", exePath, "-Wno-override-module"};
        int rc = llvm::sys::ExecuteAndWait(clang, args, std::nullopt, {}, 0, 0, &err);
        if (rc != 0) {
            diag.error(Phase::Driver, SourceLoc{}, "clang failed to build the executable" +
                                                       (err.empty() ? "" : ": " + err));
            return -1;
        }
        std::vector<llvm::StringRef> runArgs = {exePath};
        int status = llvm::sys::ExecuteAndWait(exePath, runArgs, std::nullopt, {}, 0, 0, &err);
        if (status < 0) {
            diag.error(Phase::Driver, SourceLoc{}, "could not run '" + exePath + "': " + err);
            return -1;
        }
        return status;
    }

    std::string lli = find("lli");
    if (!lli.empty()) {
        std::vector<llvm::StringRef> args = {lli, absLl.str()};
        int status = llvm::sys::ExecuteAndWait(lli, args, std::nullopt, {}, 0, 0, &err);
        if (status < 0) diag.error(Phase::Driver, SourceLoc{}, "lli failed: " + err);
        return status;
    }

    diag.error(Phase::Driver, SourceLoc{},
               "neither clang nor lli was found; install them with "
               "'pacman -S mingw-w64-ucrt-x86_64-clang mingw-w64-ucrt-x86_64-llvm'");
    return -1;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) return 2;

    if (opt.demo) {
        opt.tokens = opt.ast = opt.semantic = opt.optimizedAst = opt.llvm = opt.run = true;
    }

    std::string source;
    if (!readFile(opt.input, source)) {
        std::cerr << "Driver Error: cannot open input file '" << opt.input << "'\n";
        return 2;
    }

    Diagnostics diag(std::cerr);
    diag.setSource(source);

    if (opt.demo) {
        banner("STAGE 1: SOURCE CODE  (" + opt.input + ")");
        printNumberedSource(source);
    }

    // ---------------------------------------------------- 1. lexical analysis
    Lexer lexer(source, diag);
    std::vector<Token> tokens = lexer.tokenize();
    if (opt.tokens) {
        banner(opt.demo ? "STAGE 2: LEXICAL ANALYSIS - TOKENS" : "TOKENS");
        Lexer::printTokens(tokens, std::cout);
    }
    if (diag.hasErrors()) {
        std::cerr << "\nCompilation failed: " << diag.errorCount() << " lexical error(s).\n";
        return 1;
    }

    // ----------------------------------------------------- 2. syntax analysis
    Parser parser(tokens, diag);
    std::unique_ptr<Program> program = parser.parse();
    if (!program) {
        std::cerr << "\nCompilation failed: " << diag.errorCount() << " syntax error(s).\n";
        return 1;
    }
    if (opt.ast) {
        banner(opt.demo ? "STAGE 3: SYNTAX ANALYSIS - ABSTRACT SYNTAX TREE" : "ABSTRACT SYNTAX TREE");
        program->dump(std::cout);
    }

    // --------------------------------------------------- 3. semantic analysis
    SemanticAnalyzer sema(diag);
    bool semanticOk = sema.analyze(*program);
    if (opt.semantic) {
        banner(opt.demo ? "STAGE 4: SEMANTIC ANALYSIS - SYMBOL TABLE" : "SEMANTIC ANALYSIS - SYMBOL TABLE");
        sema.symbols().print(std::cout);
        std::cout << "\nSemantic analysis "
                  << (semanticOk ? "passed: all variables declared, all types correct.\n"
                                 : "FAILED (see errors).\n");
    }
    if (!semanticOk) {
        std::cerr << "\nCompilation failed: " << diag.errorCount()
                  << " semantic error(s). No LLVM IR was generated.\n";
        return 1;
    }

    // ---------------------------------------------------------- 4. optimizer
    Optimizer optimizer(diag);
    if (opt.optimize) optimizer.optimize(*program);
    if (opt.optimizedAst) {
        banner(opt.demo ? "STAGE 5: OPTIMIZATION" : "OPTIMIZATION");
        if (!opt.optimize) {
            std::cout << "Optimizer disabled (-O0).\n";
        } else {
            std::cout << "Transformations applied:\n";
            optimizer.printReport(std::cout);
        }
        std::cout << "\nOptimized AST:\n";
        program->dump(std::cout);
        std::cout << "\nOptimized program (as MiniLang source):\n";
        std::ostringstream src;
        printSource(*program, src);
        std::cout << (src.str().empty() ? "(empty program)\n" : src.str());
    }

    // ---------------------------------------------------- 5. LLVM IR backend
    LLVMCodeGen codegen(diag, opt.input);
    if (!codegen.generate(*program)) {
        std::cerr << "\nCompilation failed: internal code generation error.\n";
        return 1;
    }
    if (!codegen.writeIR(opt.output)) return 1;

    if (opt.llvm) {
        banner(opt.demo ? "STAGE 6: LLVM IR  (written to " + opt.output + ", verified)"
                        : "LLVM IR  (written to " + opt.output + ", verified)");
        std::cout << codegen.irText();
    }

    // --------------------------------------------------- 6. build and execute
    if (opt.run) {
        if (opt.demo) banner("STAGE 7: EXECUTION  (clang " + opt.output + " -> native executable)");
        int status = buildAndRun(opt.output, diag);
        if (status < 0) return 1;
        return status;
    }

    if (!opt.tokens && !opt.ast && !opt.semantic && !opt.optimizedAst && !opt.llvm) {
        std::cout << "Compiled '" << opt.input << "' -> " << opt.output
                  << " (LLVM IR verified). Use --run to execute it.\n";
    }
    return 0;
}
