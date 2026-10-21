# PTXRA Implementation Plan

## Architecture Overview

```
PTXRA/
├── CMakeLists.txt              # top-level CMake
├── cmake/
│   └── FindPTXAS.cmake         # optional ptxas detection
├── src/
│   ├── lexer/
│   │   ├── Token.h             # token types + Token struct
│   │   ├── Lexer.h/.cpp        # hand-written DFA lexer
│   │   └── SourceLocation.h    # file:line:col
│   ├── parser/
│   │   ├── Parser.h/.cpp       # recursive-descent parser
│   │   └── ParseError.h        # error type
│   ├── ast/
│   │   ├── AST.h               # all AST node types (Module, Kernel, Instruction…)
│   │   └── ASTPrinter.h/.cpp   # canonical pretty-printer for round-trip tests
│   ├── ir/
│   │   ├── Type.h              # PTX type enum (pred, b8…b64, u8…u64, s8…s64, f16/32/64)
│   │   ├── VirtualReg.h        # VirtualReg {id, type, regClass}
│   │   ├── Operand.h           # Operand (reg / imm / label / addr)
│   │   ├── Instruction.h       # IR instruction node
│   │   ├── BasicBlock.h        # BasicBlock with pred/succ lists
│   │   └── Function.h          # Function (kernel or device func)
│   ├── cfg/
│   │   ├── CFGBuilder.h/.cpp   # AST → IR + CFG
│   │   ├── Dominator.h/.cpp    # Cooper-Harvey-Kennedy dominators
│   │   ├── LoopInfo.h/.cpp     # natural-loop detection via back edges
│   │   └── DOTEmitter.h/.cpp   # Graphviz DOT output
│   ├── analysis/
│   │   ├── DefUse.h/.cpp       # def/use set computation
│   │   ├── Liveness.h/.cpp     # iterative liveness (live-in/live-out)
│   │   ├── LiveInterval.h/.cpp # live intervals from liveness
│   │   └── Pressure.h/.cpp     # register pressure per program point
│   ├── regalloc/
│   │   ├── PhysReg.h           # physical register model
│   │   ├── SpillManager.h/.cpp # spill-code insertion (ld.local/st.local)
│   │   ├── LinearScan.h/.cpp   # Poletto-Sarkar linear scan
│   │   ├── GraphColor.h/.cpp   # Chaitin-Briggs (simplify/spill/select)
│   │   └── Verifier.h/.cpp     # allocation correctness checker
│   ├── report/
│   │   ├── TextReport.h/.cpp   # human-readable text output
│   │   ├── JSONReport.h/.cpp   # JSON output
│   │   └── CSVReport.h/.cpp    # CSV comparison table
│   └── support/
│       └── StringUtil.h        # trim, split, format helpers
├── tools/
│   └── ptxra/
│       └── main.cpp            # CLI entry point
├── tests/
│   ├── LexerTest.cpp
│   ├── ParserTest.cpp
│   ├── CFGTest.cpp
│   ├── DominatorTest.cpp
│   ├── LoopTest.cpp
│   ├── LivenessTest.cpp
│   ├── LinearScanTest.cpp
│   ├── GraphColorTest.cpp
│   ├── VerifierTest.cpp
│   └── FuzzTest.cpp
├── samples/
│   ├── vector_add.cu / .ptx
│   ├── saxpy.cu / .ptx
│   ├── reduction.cu / .ptx
│   ├── matmul.cu / .ptx
│   ├── heavy_pressure.cu / .ptx
│   ├── divergent.cu / .ptx
│   └── stencil.cu / .ptx
├── scripts/
│   └── compare_ptxas.sh
└── docs/
    ├── ALGORITHMS.md
    ├── DECISIONS.md
    ├── INTERVIEW_NOTES.md
    └── PLAN.md                 # this file
```

## PTX Subset

### Types (RegClass)
- `pred`  → 1-bit predicate
- `b32/u32/s32` → 32-bit integer class
- `b64/u64/s64` → 64-bit integer class
- `f32`   → 32-bit float class
- `f64`   → 64-bit float class
- `b8/b16/u8/u16/s8/s16` → promoted to 32-bit class

### Register Classes (4 physical pools)
1. `PRED`  (p registers)
2. `INT32` (r registers)
3. `INT64` (rd registers)
4. `FP32`  (f registers)
5. `FP64`  (fd registers)

### Instruction Encoding (IR)
Each `Instruction` has:
- opcode (enum)
- type qualifier (PTXType)
- destination operands (0–1)
- source operands (0–3)
- predicate guard (optional VirtualReg + negation flag)
- branch target (optional label string)
- memory space (.global/.shared/.param/.local)

## Data Structures

### CFG
- `BasicBlock`: id, list of Instructions, predecessor/successor sets
- `Function`: entry block, all blocks (RPO order), VirtualReg table

### Liveness
- `LiveSet` = `std::set<VirtualRegId>` (or bitset for large kernels)
- Per block: `liveIn`, `liveOut`, `def`, `use`
- Live intervals: `{vreg, [start, end]}` in program-point space

### Register Allocation
- Physical register set: `PhysReg {class, index}` (0..N-1 per class)
- `AllocationMap`: `VirtualRegId → PhysReg | spill(slot)`

## Key Algorithms

1. **Liveness**: Classic iterative backward dataflow.
   - `liveOut[B] = ∪ liveIn[S]` for S in succ(B)
   - `liveIn[B]  = use[B] ∪ (liveOut[B] − def[B])`

2. **Linear Scan** (Poletto-Sarkar):
   - Sort intervals by start point
   - Maintain active set sorted by end point
   - Expire old intervals, spill when registers exhausted

3. **Graph Coloring** (Chaitin-Briggs):
   - Build interference graph from live intervals
   - Compute spill costs: `cost = Σ(10^depth) / degree`
   - Simplify: push colorable nodes onto stack
   - Spill: pick lowest-cost node if no colorable node
   - Select: pop stack, assign colors avoiding neighbors
   - Iterate until clean

## Milestones

| # | What | Verification |
|---|------|-------------|
| 1 | Lexer + Parser + AST printer | Round-trip parse test on all samples |
| 2 | IR lowering + CFG + DOT | DOT output correct, pred/succ consistent |
| 3 | Liveness + live intervals + pressure | Golden tests on hand-computed examples |
| 4 | Linear-scan allocator + spill | Verifier pass, register count metrics |
| 5 | Graph-coloring allocator | Verifier pass, convergence |
| 6 | Verifier | Passes on both allocators' output |
| 7 | Comparison harness + reports | CSV + markdown table from real runs |
| 8 | CLI polish + docs + tests | `ctest` green, no sanitizer errors |
