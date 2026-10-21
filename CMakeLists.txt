cmake_minimum_required(VERSION 3.16)
project(PTXRA VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Zero warnings requirement
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Module path for custom find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(PTXAS QUIET)

# Core library source files
set(PTXRA_SOURCES
    src/lexer/SourceLocation.h
    src/lexer/Token.h
    src/lexer/Token.cpp
    src/lexer/Lexer.h
    src/lexer/Lexer.cpp
    src/ast/AST.h
    src/ast/AST.cpp
    src/ast/ASTPrinter.h
    src/ast/ASTPrinter.cpp
    src/parser/ParseError.h
    src/parser/Parser.h
    src/parser/Parser.cpp
    src/ir/VirtualReg.h
    src/ir/VirtualReg.cpp
    src/ir/Operand.h
    src/ir/Operand.cpp
    src/ir/Instruction.h
    src/ir/Instruction.cpp
    src/ir/BasicBlock.h
    src/ir/Function.h
    src/ir/Function.cpp
    src/cfg/CFGBuilder.h
    src/cfg/CFGBuilder.cpp
    src/cfg/Dominator.h
    src/cfg/Dominator.cpp
    src/cfg/LoopInfo.h
    src/cfg/LoopInfo.cpp
    src/cfg/DOTEmitter.h
    src/cfg/DOTEmitter.cpp
    src/analysis/Liveness.h
    src/analysis/Liveness.cpp
    src/analysis/LiveInterval.h
    src/analysis/LiveInterval.cpp
    src/analysis/Pressure.h
    src/analysis/Pressure.cpp
    src/regalloc/PhysReg.h
    src/regalloc/PhysReg.cpp
    src/regalloc/AllocationMap.h
    src/regalloc/AllocationMap.cpp
    src/regalloc/SpillManager.h
    src/regalloc/SpillManager.cpp
    src/regalloc/LinearScan.h
    src/regalloc/LinearScan.cpp
    src/regalloc/GraphColor.h
    src/regalloc/GraphColor.cpp
    src/regalloc/Verifier.h
    src/regalloc/Verifier.cpp
    src/report/TextReport.h
    src/report/TextReport.cpp
    src/report/JSONReport.h
    src/report/JSONReport.cpp
    src/report/CSVReport.h
    src/report/CSVReport.cpp
    src/support/StringUtil.h
)

add_library(ptxra_core STATIC ${PTXRA_SOURCES})
target_include_directories(ptxra_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# CLI tool
add_executable(ptxra tools/ptxra/main.cpp)
target_link_libraries(ptxra PRIVATE ptxra_core)

# Tests
option(BUILD_TESTING "Build the test suite" ON)
if(BUILD_TESTING)
    include(CTest)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    # Helper function to define tests
    function(ptxra_add_test TEST_NAME TEST_SOURCE)
        add_executable(${TEST_NAME} ${TEST_SOURCE})
        target_link_libraries(${TEST_NAME} PRIVATE ptxra_core GTest::gtest_main)
        target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endfunction()

    ptxra_add_test(LexerTest tests/LexerTest.cpp)
    ptxra_add_test(ParserTest tests/ParserTest.cpp)
    ptxra_add_test(CFGTest tests/CFGTest.cpp)
    ptxra_add_test(DominatorTest tests/DominatorTest.cpp)
    ptxra_add_test(LoopTest tests/LoopTest.cpp)
    ptxra_add_test(LivenessTest tests/LivenessTest.cpp)
    ptxra_add_test(LinearScanTest tests/LinearScanTest.cpp)
    ptxra_add_test(GraphColorTest tests/GraphColorTest.cpp)
    ptxra_add_test(VerifierTest tests/VerifierTest.cpp)
    ptxra_add_test(FuzzTest tests/FuzzTest.cpp)
endif()
