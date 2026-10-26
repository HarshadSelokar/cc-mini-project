# =============================================================================
#  MiniLang Compiler -- plain Makefile (alternative to CMake)
#
#  From an MSYS2 UCRT64 shell:
#      make            build bin/minilang.exe
#      make test       run the test suite
#      make demo       run the step-by-step demonstration
#      make clean      remove build output
#
#  LLVM flags come from llvm-config.  Paths are relative on purpose: make
#  cannot handle spaces in absolute paths (e.g. "D:/7th Sem/...").
# =============================================================================

CXX         ?= g++
FLEX        ?= flex
BISON       ?= bison
LLVM_CONFIG ?= llvm-config

UNAME := $(shell uname -s)
ifneq (,$(filter Windows_NT,$(OS))$(findstring MINGW,$(UNAME))$(findstring MSYS,$(UNAME)))
  EXE := .exe
endif

OBJDIR := build-make
GENDIR := $(OBJDIR)/generated
TARGET := bin/minilang$(EXE)

LLVM_CXXFLAGS := $(filter-out -std=%,$(shell $(LLVM_CONFIG) --cxxflags))
LLVM_LDFLAGS  := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LIBS     := $(shell $(LLVM_CONFIG) --libs core support) $(shell $(LLVM_CONFIG) --system-libs)
LLVM_BINDIR   := $(shell $(LLVM_CONFIG) --bindir)

CXXFLAGS ?= -O2
CXXFLAGS += -std=c++17 -Wall -Iinclude -I$(GENDIR) $(LLVM_CXXFLAGS) \
            -DMINILANG_LLVM_BINDIR='"$(LLVM_BINDIR)"' -DMINILANG_VERSION='"1.0"'

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(SOURCES)) \
           $(OBJDIR)/parser.tab.o $(OBJDIR)/lexer.yy.o

.PHONY: all test demo clean check-env

all: $(TARGET)

$(TARGET): $(OBJECTS) | bin
	$(CXX) $(OBJECTS) -o $@ $(LLVM_LDFLAGS) $(LLVM_LIBS)
	@echo "Built $@"

# ---- generated parser and scanner ----------------------------------------
$(GENDIR)/parser.tab.cpp $(GENDIR)/parser.tab.hpp: parser/parser.y | $(GENDIR)
	$(BISON) -Wall -d -o $(GENDIR)/parser.tab.cpp $<

$(GENDIR)/lexer.yy.cpp: parser/lexer.l $(GENDIR)/parser.tab.hpp | $(GENDIR)
	$(FLEX) -o $@ $<

$(OBJDIR)/parser.tab.o: $(GENDIR)/parser.tab.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/lexer.yy.o: $(GENDIR)/lexer.yy.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- compiler sources (all depend on the generated token header) ---------
$(OBJDIR)/%.o: src/%.cpp $(wildcard include/*.h) $(GENDIR)/parser.tab.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR) $(GENDIR) bin:
	mkdir -p $@

# ---- convenience targets --------------------------------------------------
check-env:
	@echo "g++:   $$($(CXX) --version | head -n 1)"
	@echo "flex:  $$($(FLEX) --version)"
	@echo "bison: $$($(BISON) --version | head -n 1)"
	@echo "llvm:  $$($(LLVM_CONFIG) --version)"
	@echo "clang: $$(clang --version | head -n 1)"

test: $(TARGET)
	bash tests/run_tests.sh $(TARGET)

demo: $(TARGET)
	./$(TARGET) examples/demo.ml --demo -o output/demo.ll

clean:
	rm -rf $(OBJDIR) $(TARGET) output/*.ll output/*.exe
