# Changelog

All notable changes to the PTXRA project are documented below, organized by milestone.

---

## [Unreleased] - 2026-10-20

### Milestone 1: Front-End (Lexer, Parser, AST, Pretty-Printer)
- Implemented hand-written DFA `Lexer` with token kinds for PTX directives, registers, labels, integers, floats, and operators.
- Added robust comment stripping (`//` and `/* */`) and source location tracking (`file:line:col`).
- Implemented recursive-descent `Parser` supporting PTX modules, kernels, declarations (`.reg`, `.shared`, `.local`), and instructions.
- Implemented `ASTPrinter` for canonical pretty-printing and verified round-trip AST identity.
- Added comprehensive unit tests in `LexerTest.cpp` and `ParserTest.cpp`.

### Milestone 2: IR Lowering & CFG Construction
- Defined 3-address style IR (`ir::Function`, `ir::BasicBlock`, `ir::Instruction`, `ir::Operand`, `ir::VirtualReg`).
- Implemented `cfg::CFGBuilder` supporting fall-through semantics, conditional predicated branches (`@%p0 bra`), multiple exits, and unreachable blocks.
- Implemented `cfg::DominatorTree` using Cooper-Harvey-Kennedy dominator algorithm in Reverse Post-Order (RPO).
- Implemented `cfg::LoopInfo` detecting natural loops via back-edges and computing loop nesting depth.
- Implemented `cfg::DOTEmitter` generating Graphviz DOT output with block record labels.
- Added unit tests in `CFGTest.cpp`, `DominatorTest.cpp`, and `LoopTest.cpp`.

### Milestone 3: Liveness Analysis & Register Pressure
- Implemented iterative backward dataflow analysis in `analysis::LivenessAnalysis` computing `def`, `use`, `liveIn`, and `liveOut` sets per block.
- Implemented `analysis::LiveIntervals` computing contiguous `[start, end]` ranges with program-point numbering ($2i$ use, $2i+1$ def).
- Implemented `analysis::PressureResult` tracking peak register pressure overall and per register class (`pred`, `int32`, `int64`, `float32`, `float64`).
- Added golden tests in `LivenessTest.cpp`.

### Milestone 4: Linear Scan Register Allocator
- Implemented Poletto-Sarkar linear scan in `regalloc::LinearScanAllocator` with configurable physical register budgets per class.
- Implemented active interval expiration and furthest-endpoint eviction heuristic.
- Implemented `regalloc::SpillManager` inserting `ld.local` and `st.local` spill operations into the IR.
- Added tests in `LinearScanTest.cpp`.

### Milestone 5: Graph Coloring Register Allocator
- Implemented Chaitin-Briggs graph coloring in `regalloc::GraphColorAllocator`.
- Built interference graphs from overlapping live intervals per register class.
- Implemented loop-depth weighted spill cost heuristic: $\sum 10^{\text{depth}} / \text{degree}$.
- Implemented Simplify, optimistic Spill, and Select phases.
- Added golden tests in `GraphColorTest.cpp`.

### Milestone 6: Formal Verification Pass
- Implemented `regalloc::AllocationVerifier` mathematically proving allocation correctness.
- Checks:
  1. No two simultaneously live virtual registers share a physical register.
  2. Physical register classes match virtual register classes.
  3. Register indices are within limits.
  4. Every active virtual register is either assigned or spilled.
- Added unit tests in `VerifierTest.cpp`.

### Milestone 7: Comparison Harness & Benchmarking
- Implemented `report::ComparisonReporter` emitting Markdown and CSV comparison tables.
- Integrated optional detection of `ptxas` / `nvcc` via `FindPTXAS.cmake`.
- Created `scripts/compare_ptxas.sh` compiling `.cu` samples and parsing `ptxas -v` output.
- Hand-crafted 7 realistic sample kernels (`vector_add`, `saxpy`, `reduction`, `matmul`, `heavy_pressure`, `divergent`, `stencil`).

### Milestone 8: CLI Tool, Documentation & Quality Assurance
- Built `ptxra` CLI supporting `parse`, `cfg`, `liveness`, `pressure`, `alloc`, and `compare` with `--json`, `--dot`, and `--max-regs=N`.
- Created property/fuzz tests in `FuzzTest.cpp` generating randomized straight-line and branching IR.
- Verified zero compiler warnings under `-Wall -Wextra -Wpedantic -Werror`.
- Verified zero sanitizer errors under `-fsanitize=address,undefined`.
- Refactored register allocation to a ptxas-style unified 32-bit slot model with 64-bit even-alignment, scratch slot reservations, separated predicates, and strict budget bounds.
- Documented project in `README.md`, `docs/ALGORITHMS.md`, `docs/DECISIONS.md`, and `docs/INTERVIEW_NOTES.md`.

