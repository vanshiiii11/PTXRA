# PTXRA: PTX Register Allocator & Analysis Toolkit

PTXRA is an end-to-end NVIDIA PTX compiler toolkit written in **modern C++17** with zero external dependencies (GoogleTest fetched only for test suites). It parses PTX assembly, builds Control-Flow Graphs (CFGs), computes dominator trees and loop nesting structures, runs backward liveness analysis to calculate register pressure per program point, and implements two classic register allocators:
1. **Linear Scan (Poletto & Sarkar)**
2. **Graph Coloring with Optimistic Spilling (Chaitin-Briggs)**

Every allocation is automatically verified against four mathematical invariants via an integrated formal verifier.

---

## Architecture

```mermaid
flowchart TD
    PTX["PTX Source (*.ptx)"] --> Lexer["Lexer\n(DFA Tokenizer)"]
    Lexer --> Parser["Parser\n(Recursive Descent)"]
    Parser --> AST["AST & Pretty-Printer\n(Round-Trip Validated)"]
    AST --> CFG["CFG Builder\n(Fall-through & Predicated Branches)"]
    CFG --> Dom["Dominators & LoopInfo\n(Cooper-Harvey-Kennedy & Back-Edges)"]
    CFG --> Liveness["Liveness Analysis\n(Iterative Backward Dataflow)"]
    Liveness --> Intervals["Live Intervals & Pressure\n(Per-Point & Per-Class Tracking)"]
    
    Intervals --> LS["Linear Scan Allocator\n(Furthest-End Eviction)"]
    Intervals --> GC["Graph Coloring Allocator\n(Chaitin-Briggs / Loop-Weighted)"]
    
    LS --> Verifier["Allocation Verifier\n(Interference & Bound Checker)"]
    GC --> Verifier
    
    Verifier --> SpillMgr["Spill Manager\n(Local Memory Spill Rewriting)"]
    Verifier --> Reports["Reports\n(Text, JSON, DOT, CSV/Markdown Tables)"]
```

---

## Features

- **Strict C++17 Standard**: Compiles with `-Wall -Wextra -Wpedantic -Werror` with zero warnings.
- **Robust Front-End**: Supports `.version`, `.target`, `.address_size`, `.visible`, `.entry`, `.func`, `.param`, `.reg` (ranges `%r<10>` and discrete), `.shared`, `.local`, instructions with modifier dot-chains (`mad.lo.s32`, `fma.rn.f32`, `st.global.f32`), and predicated execution (`@%p0`, `@!%p0`).
- **Control-Flow Analysis**:
  - Cooper-Harvey-Kennedy dominators in Reverse Post-Order (RPO).
  - Natural loop detection via back-edge identification with nesting depths.
  - Graphviz DOT emission with detailed basic block information.
- **Liveness & Pressure**:
  - Block-level $\text{def}$, $\text{use}$, $\text{liveIn}$, $\text{liveOut}$ sets.
  - Fine-grained program-point intervals ($2i$ use, $2i+1$ def).
  - Peak register pressure tracking across 5 register classes: `pred`, `int32`, `int64`, `float32`, `float64`.
- **Allocators**:
  - **Linear Scan**: Fast $O(N \log N)$ interval traversal with furthest-end eviction.
  - **Graph Coloring**: Chaitin-Briggs interference graph with loop-depth weighted spill costs ($10^{\text{depth}} / \text{degree}$), Simplify, optimistic Spill, and Select phases.
- **Formal Verifier**: Guarantees zero overlapping live ranges share physical registers, enforces register class constraints, and verifies spill consistency.
- **ptxas Compatibility Harness**: Optional detection of CUDA toolkit (`nvcc` / `ptxas -v`); pre-generated sample `.ptx` files enable standalone execution on any machine.

---
## Allocation Model (ptxas-style)

- **Budget** `--max-regs=N` is a pool of N 32-bit register slots shared by int32/f32 values.
- **64-bit values** (int64/f64) take an even-aligned pair of slots.
- **Predicates** use a separate pool of 7 and are reported separately.
- **Registers used** = highest slot used + 1.
- **Spilling** reserves 2 scratch slots *inside* the budget for reloads.
- **Linear scan** works on contiguous live intervals. **Graph coloring** builds its interference graph from precise per-point liveness, so the two can differ in both slot count and spill choices.

## Build & Test Instructions

### Requirements
- C++17 compliant compiler (`clang++` $\ge$ 10 or `g++` $\ge$ 9)
- CMake $\ge$ 3.16

### Building from Source

```bash
# Configure and build in Release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Run the full test suite (10 test suites)
ctest --test-dir build --output-on-failure
```

### Running with Sanitizers (ASan + UBSan)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

---

## CLI Usage

The `ptxra` binary provides six subcommands:

```bash
# 1. Parse and print summary (or --print for canonical PTX round-trip)
./build/ptxra parse samples/vector_add.ptx

# 2. Control-flow graph report (or --dot for Graphviz visual output)
./build/ptxra cfg samples/vector_add.ptx --dot > vector_add.dot

# 3. Liveness analysis (live-in / live-out sets per block)
./build/ptxra liveness samples/vector_add.ptx

# 4. Register pressure analysis (peak overall and per register class)
./build/ptxra pressure samples/vector_add.ptx

# 5. Register allocation with automatic verification
./build/ptxra alloc samples/heavy_pressure.ptx --algo=graph --max-regs=8

# 6. Comparative benchmark table across allocators
./build/ptxra compare samples/heavy_pressure.ptx --max-regs=8
```

---
## Key Results (Budget = 16 slots, weighted spill cost, lower is better)

| Kernel | Linear Scan | Graph Coloring | Note |
| :--- | :---: | :---: | :--- |
| `matmul` | 437 | **13** | GC spills outside the hot loop |
| `nested_loops` | 247 | **106** | loop-depth weighting helps |
| `heavy_pressure` | 229 | **129** | |
| `fma_chain` | 75 | 74 | straight-line code, no loop signal |

At budgets ≥ 24, GC usually needs fewer slots (e.g. 22 vs 26 on `heavy_pressure`). It is **not** uniformly better: on `nested_loops` it uses 19 slots vs linear scan's 18, caused by 64-bit pair alignment interacting with coloring order. Linear scan here is also not alignment-aware, so some of GC's slot advantage reflects an implementation gap, not only the algorithm.

## Benchmark Comparison Summary

Real runs across all 11 realistic & stress kernels in `samples/` comparing **Linear Scan** vs **Graph Coloring** across 5 budgets (16, 24, 32, 64, 255):

> The full 110-entry evaluation dataset is available in [`benchmark_results.md`](benchmark_results.md) and [`benchmark_results.csv`](benchmark_results.csv).

### Evaluation Overview

| Kernel | Budget | Allocator | Slots Used | Spill Stores | Spill Loads | Weighted Spill Cost |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| `vector_add` | 16 / 24 / 32 / 64 / 255 | LinearScan | 8 | 0 | 0 | 0 |
| `vector_add` | 16 / 24 / 32 / 64 / 255 | GraphColoring | 8 | 0 | 0 | 0 |
| `saxpy` | 16 / 24 / 32 / 64 / 255 | LinearScan | 8 | 0 | 0 | 0 |
| `saxpy` | 16 / 24 / 32 / 64 / 255 | GraphColoring | 8 | 0 | 0 | 0 |
| `divergent` | 16 / 24 / 32 / 64 / 255 | LinearScan | 8 | 0 | 0 | 0 |
| `divergent` | 16 / 24 / 32 / 64 / 255 | GraphColoring | 6 | 0 | 0 | 0 |
| `block_reduce` | 16 / 24 / 32 / 64 / 255 | LinearScan | 10 | 0 | 0 | 0 |
| `block_reduce` | 16 / 24 / 32 / 64 / 255 | GraphColoring | 8 | 0 | 0 | 0 |
| `stencil_2d` | 16 / 24 / 32 / 64 / 255 | LinearScan | 16 | 0 | 0 | 0 |
| `stencil_2d` | 16 / 24 / 32 / 64 / 255 | GraphColoring | 13 | 0 | 0 | 0 |
| `live_range_hole` | 16 / 24 / 32 / 64 / 255 | LinearScan | 12 | 0 | 0 | 0 |
| `live_range_hole` | 16 / 24 / 32 / 64 / 255 | GraphColoring | 8 | 0 | 0 | 0 |
| `long_lived` | 16 / 24 / 32 / 64 / 255 | LinearScan | 15 | 0 | 0 | 0 |
| `long_lived` | 16 / 24 / 32 / 64 / 255 | GraphColoring | 11–12 | 0 | 0 | 0 |
| `matmul` | 16 | LinearScan | 16 | 10 | 22 | 437 |
| `matmul` | 16 | GraphColoring | 16 | 2 | 2 | 13 |
| `matmul` | 24 / 32 / 64 / 255 | LinearScan | 20 | 0 | 0 | 0 |
| `matmul` | 24 / 32 / 64 / 255 | GraphColoring | 17 | 0 | 0 | 0 |
| `heavy_pressure` | 16 | LinearScan | 16 | 16 | 24 | 229 |
| `heavy_pressure` | 16 | GraphColoring | 16 | 10 | 29 | 129 |
| `heavy_pressure` | 24 | LinearScan | 24 | 2 | 3 | 5 |
| `heavy_pressure` | 24 / 32 / 64 / 255 | GraphColoring | 22 | 0 | 0 | 0 |
| `heavy_pressure` | 32 / 64 / 255 | LinearScan | 26 | 0 | 0 | 0 |
| `nested_loops` | 16 | LinearScan | 16 | 6 | 7 | 247 |
| `nested_loops` | 16 | GraphColoring | 16 | 3 | 4 | 106 |
| `nested_loops` | 24 / 32 / 64 / 255 | LinearScan | 18 | 0 | 0 | 0 |
| `nested_loops` | 24 / 32 / 64 / 255 | GraphColoring | 19 | 0 | 0 | 0 |
| `fma_chain` | 16 | LinearScan | 16 | 37 | 38 | 75 |
| `fma_chain` | 16 | GraphColoring | 16 | 37 | 37 | 74 |
| `fma_chain` | 24 | LinearScan | 24 | 29 | 30 | 59 |
| `fma_chain` | 24 | GraphColoring | 24 | 29 | 29 | 58 |
| `fma_chain` | 32 | LinearScan | 32 | 21 | 22 | 43 |
| `fma_chain` | 32 | GraphColoring | 32 | 21 | 21 | 42 |
| `fma_chain` | 64 / 255 | LinearScan | 52 | 0 | 0 | 0 |
| `fma_chain` | 64 / 255 | GraphColoring | 52 | 0 | 0 | 0 |

---

## Test Coverage (gcov)

Line coverage measured with `gcov` across all source components:

| Component | Lines Covered | Total Lines | Line Coverage |
| :--- | :---: | :---: | :---: |
| `src/analysis` | 149 | 155 | 96.1% |
| `src/cfg` | 335 | 367 | 91.3% |
| `src/regalloc` | 486 | 580 | 83.8% |
| `src/parser` | 298 | 402 | 74.1% |
| `src/ir` | 131 | 193 | 67.9% |
| `src/report` | 105 | 161 | 65.2% |
| `src/ast` | 186 | 293 | 63.5% |
| `src/lexer` | 244 | 411 | 59.4% |
| **Total** | **1934** | **2562** | **75.5%** |

---

## Limitations

## Limitations

1. **Hand-written PTX samples**: modeled on `nvcc` output, not generated by it.
2. **No `ptxas` comparison yet**: no CUDA toolkit on the development machine. `scripts/compare_ptxas.sh` automates this where `nvcc` exists. `ptxas` numbers would be indicative only, since it optimizes and schedules after allocation.
3. **No GPU execution**: offline static analysis only.
4. **Non-SSA IR**: linear scan uses bounding-box intervals; graph coloring uses per-point liveness.
5. **Static spill counts**: reported load/store counts are static; use the loop-weighted cost for comparisons.
6. **Fixed 2 scratch slots**: instructions with more than 2 simultaneously spilled operands may need more (test pending).
7. **Simplify uses neighbor count**: it may not use slot-weighted degree for 64-bit nodes (under review).
8. **Coverage**: 75.5% overall (lexer 59.4%, measured with `gcov` on macOS). Sanitizer runs are ASan+UBSan; leak detection is limited on macOS.
9. **Not modeled**: bank conflicts, dual-issue constraints, rematerialization, coalescing, live-range splitting.

---

## Future Work & Concrete Next Steps

1. **Live-Range Splitting**: Split long live ranges across loop boundaries so values can spill during inner loops and reload afterward, drastically reducing spill traffic.
2. **Conservative Coalescing (George & Briggs criteria)**: Merge non-interfering copy instruction operands (`mov %r1, %r0`) to eliminate redundant register-to-register moves.
3. **Rematerialization**: Identify inexpensive, side-effect-free instructions (constants, address calculations, thread indices `%tid.x`) and recompute them rather than spilling and reloading from memory.
