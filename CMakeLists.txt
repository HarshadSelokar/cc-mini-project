# =============================================================================
#  MiniLang Compiler -- CMake build
#
#  From an MSYS2 UCRT64 shell:
#      cmake -S . -B build
#      cmake --build build
#  The executable is written to bin/minilang.exe.
#
#  Requirements: g++ (C++17), flex, bison >= 3.6, LLVM (llvm-config on PATH).
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(MiniLangCompiler VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ----------------------------------------------------------- Flex / Bison ----
find_package(FLEX REQUIRED)
find_package(BISON 3.6 REQUIRED)     # 3.6+ needed for "parse.error custom"

set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

BISON_TARGET(MiniLangParser ${CMAKE_CURRENT_SOURCE_DIR}/parser/parser.y
             ${GEN_DIR}/parser.tab.cpp
             DEFINES_FILE ${GEN_DIR}/parser.tab.hpp)
FLEX_TARGET(MiniLangLexer ${CMAKE_CURRENT_SOURCE_DIR}/parser/lexer.l
            ${GEN_DIR}/lexer.yy.cpp)
ADD_FLEX_BISON_DEPENDENCY(MiniLangLexer MiniLangParser)

# ------------------------------------------------------- LLVM (llvm-config) --
find_program(LLVM_CONFIG_EXECUTABLE NAMES llvm-config)
if(NOT LLVM_CONFIG_EXECUTABLE)
  message(FATAL_ERROR "llvm-config not found. Install LLVM: "
                      "pacman -S mingw-w64-ucrt-x86_64-llvm")
endif()

function(llvm_config out_var)
  execute_process(COMMAND ${LLVM_CONFIG_EXECUTABLE} ${ARGN}
                  OUTPUT_VARIABLE output
                  OUTPUT_STRIP_TRAILING_WHITESPACE
                  RESULT_VARIABLE status)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "llvm-config ${ARGN} failed")
  endif()
  set(${out_var} "${output}" PARENT_SCOPE)
endfunction()

llvm_config(LLVM_VERSION     --version)
llvm_config(LLVM_CXXFLAGS    --cxxflags)
llvm_config(LLVM_LDFLAGS     --ldflags)
llvm_config(LLVM_LIBS        --libs core support)
llvm_config(LLVM_SYSTEM_LIBS --system-libs)
llvm_config(LLVM_BINDIR      --bindir)

message(STATUS "Found LLVM ${LLVM_VERSION} (${LLVM_CONFIG_EXECUTABLE})")
message(STATUS "LLVM tools directory: ${LLVM_BINDIR}")

separate_arguments(LLVM_CXXFLAGS_LIST UNIX_COMMAND "${LLVM_CXXFLAGS}")
separate_arguments(LLVM_LINK_LIST UNIX_COMMAND "${LLVM_LDFLAGS} ${LLVM_LIBS} ${LLVM_SYSTEM_LIBS}")
# The C++ standard is chosen by CMAKE_CXX_STANDARD above.
list(FILTER LLVM_CXXFLAGS_LIST EXCLUDE REGEX "^-std=")

# ------------------------------------------------------------- compiler ------
add_executable(minilang
  src/main.cpp
  src/Diagnostics.cpp
  src/Lexer.cpp
  src/Parser.cpp
  src/AST.cpp
  src/SymbolTable.cpp
  src/SemanticAnalyzer.cpp
  src/Optimizer.cpp
  src/LLVMCodeGen.cpp
  ${BISON_MiniLangParser_OUTPUTS}
  ${FLEX_MiniLangLexer_OUTPUTS}
)

target_include_directories(minilang PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${GEN_DIR})
target_compile_options(minilang PRIVATE ${LLVM_CXXFLAGS_LIST} -Wall)
target_compile_definitions(minilang PRIVATE
  MINILANG_LLVM_BINDIR="${LLVM_BINDIR}"
  MINILANG_VERSION="${PROJECT_VERSION}")
target_link_libraries(minilang PRIVATE ${LLVM_LINK_LIST})

set_target_properties(minilang PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# ---------------------------------------------------------------- tests ------
enable_testing()
find_program(BASH_EXECUTABLE NAMES bash)
if(BASH_EXECUTABLE)
  add_test(NAME minilang_test_suite
           COMMAND ${BASH_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tests.sh
                   $<TARGET_FILE:minilang>
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
