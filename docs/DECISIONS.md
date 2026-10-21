# Architectural & Design Decisions

This document records ambiguous or non-trivial design decisions made during the implementation of PTXRA, along with rationale and tradeoffs.

---

## 1. PTX Parser & Lexer Strategy

### Decision
Use a hand-written DFA lexer and recursive-descent parser rather than Flex/Bison or ANTLR.
### Rationale
- Zero external runtime or build-time dependencies beyond standard C++17.
- Precise `file:line:column` diagnostics and clean error recovery for syntax errors.
- PTX uses dot-prefixed keywords (`.version`, `.reg`, `.entry`) and instructions with modifier dot-chains (`ld.global.f32`, `mul.wide.s32`, `setp.ge.s32`). A hand-written lexer cleanly parses compound instructions and classifies qualifiers in context.
- Unsupported or future PTX instructions produce an `UnsupportedFeature` exception containing the exact source location rather than crashing.

---

## 2. Register Classification & Promotion: ptxas-Style Unified 32-Bit Slot Model

### Decision
Model physical registers as a unified pool of $N$ 32-bit register slots (configurable via `--max-regs=N`, default 255) shared across integer and floating-point types:
- **32-bit registers** (`INT32` / `FLOAT32`): Occupy 1 slot ($s \in [0, N)$).
- **64-bit registers** (`INT64` / `FLOAT64`): Occupy an aligned pair of consecutive slots ($s, s+1$) where $s$ is even ($s \pmod 2 = 0$).
- **Predicates** (`PRED`): Allocated from an independent pool of 7 1-bit predicate registers (`%p0`..`%p6`), matching SM hardware.
- **Scratch Reservation**: When spilling occurs, 2 scratch slots are reserved for spill loads/stores and counted in the total slots used.
- **Registers Used**: Defined strictly as $\max(\text{slot} + \text{width})$, i.e., highest slot index used + 1. Predicates are reported separately.

### Rationale
- Faithfully mirrors the physical register architecture of NVIDIA GPUs (`ptxas`), where 32-bit hardware registers form the basic assignable unit and 64-bit values require register pairing with even-odd alignment.
- Eliminates artificial per-class limits (e.g. `n/2` float64 heuristic) in favor of a single unified budget.

---

## 3. CFG Construction & Fall-Through Semantics

### Decision
Basic blocks are demarcated by explicit labels and terminator instructions (`bra`, `ret`, `exit`). Predicated branches (`@%p0 bra L1`) create two successor edges: one to the branch target `L1`, and one fall-through edge to the immediately succeeding basic block.
### Rationale
- GPUs execute warps where divergent branches cause SIMT reconvergence; in a scalar CFG model, a predicated branch behaves like a conditional branch with both a taken target and a fall-through target.

---

## 4. Dominator Tree Algorithm

### Decision
Cooper-Harvey-Kennedy iterative dominator algorithm ("A Simple, Fast Dominance Algorithm", 2001) using Reverse Post-Order (RPO) numbering.
### Rationale
- Much simpler to implement and debug than Lengauer-Tarjan while running faster in practice on reducible and small-to-medium CFGs typical of GPU kernels.
- Predecessors are intersected using efficient RPO finger-walking.

---

## 5. Loop Nesting Depth & Natural Loops

### Decision
Natural loops are detected by finding back-edges $(u \to v)$ where $v$ dominates $u$. The loop body is collected by backward BFS from $u$ stopping at header $v$. Nesting depth is assigned by counting how many natural loop bodies contain each basic block.
### Rationale
- Standard compiler definition (Dragon Book). Handles nested loops cleanly and provides the exact loop depth exponent ($10^{\text{depth}}$) used in Chaitin-Briggs spill cost heuristics.

---

## 6. Liveness Representation: Contiguous Intervals (Linear Scan) vs. Precise Per-Point Liveness (Graph Coloring)

### Decision
- **Linear Scan**: Uses contiguous `[start, end]` live intervals constructed from program-point numbering ($2i$ use, $2i+1$ def). Live ranges are conservative bounding spans across the linear schedule.
- **Graph Coloring**: Reconstructs exact per-point liveness via backward dataflow traversal within each basic block from `liveOut`. An interference edge $(d, v)$ is added if and only if $v$ is genuinely live at the definition point of $d$ within the same register file/pool.
### Rationale
- Standard linear scan (Poletto-Sarkar) algorithmically relies on sorting intervals by contiguous start points and expiring them sequentially by end points.
- Contiguous intervals overestimate interference across control-flow holes and disjoint branches (e.g. if a variable is live in block A and block C, but completely dead in block B, contiguous interval falsely considers it live throughout B).
- Graph coloring with per-point backward traversal eliminates these spurious interferences, allowing disjoint variables on alternative control flow paths to share registers without artificial interference edges.

---

## 7. Linear Scan: Eviction Policy

### Decision
When the physical register pool is exhausted for a register class, evict the candidate with the furthest end-point (either the current interval or an interval currently assigned in the active set).
### Rationale
- Classic Poletto-Sarkar heuristic minimizing subsequent register pressure and maximizing the distance to the next potential spill.

---

## 8. Graph Coloring: Chaitin-Briggs with Precise Per-Point Interference

### Decision
Build interference graphs from precise per-point backward liveness walks across instructions in each block. Use Chaitin-Briggs with:
- Simplify phase: remove nodes with $\text{degree} < K$.
- Optimistic spill phase: if all nodes have $\text{degree} \ge K$, select the node with the lowest loop-weighted spill cost $\sum 10^{\text{depth}} / (\text{degree} \times \text{width})$ and push it to the stack.
- Select phase: pop and color into lowest available slot (respecting even alignment for 64-bit); if no valid color exists among neighbors, assign a spill slot.
### Rationale
- Precise per-point interference prevents false edge creation across CFG diamonds, if-then-else branches, and dead zones.
- Loop-depth weighted cost prioritizes spilling straight-line code outside hot loops, drastically reducing dynamic memory traffic.

---

## 9. Local Spill Code Insertion

### Decision
Spilled registers are assigned unique spill slots in `.local` address space. When rewriting code:
- Stores (`st.local`) are placed immediately after each definition.
- Loads (`ld.local`) are placed immediately before each use.
### Rationale
- Directly mirrors how PTX and CUDA local memory spills work without requiring full machine-instruction scheduling.

---

## 10. Allocator Divergence & Convergence Analysis: Where Linear Scan and Graph Coloring Differ vs. Match

### Where They Differ:
1. **Branching Code with Live-Range Holes (`live_range_hole`, `divergent`)**:
   - **Why**: Graph Coloring uses precise per-point liveness walks, recognizing when a variable is completely dead in alternative branches (e.g. `else` block). Linear Scan uses contiguous intervals $[start, end]$ that bridge across the entire control flow structure, artificially inflating register pressure (`live_range_hole` uses **8 slots** in GC vs. **12 slots** in LS).
2. **64-Bit Pair-Alignment Fragmentation (`stencil_2d`, `matmul`)**:
   - **Why**: 64-bit values require even slot indices ($s \pmod 2 == 0$). Linear Scan's greedy chronological allocation encounters odd-slot holes that cannot hold 64-bit pairs, skipping slots and inflating peak register usage (**16 slots** in LS vs. **13 slots** in GC on `stencil_2d`). Graph Coloring simplifies low-degree variables first and colors high-degree (aligned 64-bit) pairs first in the select phase, tightly packing pairs into lower even slots.
3. **Loop-Weighted Spill Costs Under Pressure (`matmul`, `heavy_pressure`, `nested_loops` at Budget 16)**:
   - **Why**: Linear Scan evicts the candidate with the furthest end-point, which frequently spills loop-hot variables inside deep loops. Graph Coloring weights eviction by $\sum 10^{\text{depth}} / (\text{degree} \times \text{width})$, protecting inner loop variables and spilling cold/pre-loop variables instead. On `matmul` at Budget 16, Linear Scan incurs a weighted spill cost of **437**, whereas Graph Coloring incurs a spill cost of just **13** (a 33x reduction in dynamic memory traffic). On `nested_loops`, GC achieves **106** cost vs. LS **247**.

### Where They Do Not Differ:
1. **Low-Pressure Straight-Line Kernels (`vector_add`, `saxpy`)**:
   - **Why**: Register pressure is well below physical capacity (8 slots needed vs. budgets $\ge 16$). Both allocators find the trivial zero-spill optimal assignment.
2. **Dense Straight-Line Interference Cliques Without Loops (`fma_chain`)**:
   - **Why**: In `fma_chain`, 48 variables are simultaneously live in basic block 0 (depth 0, weight 1). The interference graph is a dense clique where every variable interferes with every other variable. Because there are no loops to guide spill heuristics and no live-range holes, both allocators are mathematically forced to spill the exact same number of variables at each constrained budget (e.g., exactly 21 spills at Budget 32, 29 spills at Budget 24, 37 spills at Budget 16).

