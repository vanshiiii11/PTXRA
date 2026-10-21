# PTXRA Interview & Technical Deep-Dive Notes

This document contains interview-ready explanations, design retrospectives, and deep technical answers regarding compiler architecture, PTX, and register allocation.

---

## 1. The 30-Second Pitch

> "PTXRA is an end-to-end PTX compiler middle-end and backend toolkit built in C++17 with zero dependencies. It parses NVIDIA PTX assembly, constructs explicit control-flow graphs, computes Cooper-Harvey-Kennedy dominators and natural loops, runs iterative backward liveness analysis to calculate register pressure, and implements both Poletto-Sarkar Linear Scan and Chaitin-Briggs Graph Coloring register allocators with local spill-code rewriting and formal allocation verification. It provides an extensible CLI with JSON, Graphviz DOT, and CSV/Markdown reports comparing allocation efficiency against NVIDIA's `ptxas`."

---

## 2. The 2-Minute Architectural Walkthrough

> "The pipeline consists of six distinct, decoupled phases:
>
> 1. **Front-End & AST**: A hand-written DFA lexer tokenizes PTX directives, registers, and instructions. The recursive-descent parser produces an Abstract Syntax Tree with complete source-location tracking and canonical round-trip pretty-printing (`ast::ASTPrinter`).
> 2. **IR Lowering & CFG**: The AST lowers to a 3-address style IR (`ir::Function`, `ir::BasicBlock`, `ir::Instruction`). We identify leaders, connect fall-through and predicated branch edges (`@%p0 bra`), compute Cooper-Harvey-Kennedy dominators in RPO, and find natural loop nesting depth via back-edges.
> 3. **Liveness & Pressure**: A backward dataflow analysis computes block `use`, `def`, `liveIn`, and `liveOut` sets until a fixed point is reached. Program points ($2i$ for use, $2i+1$ for def) produce contiguous `LiveInterval`s and fine-grained register pressure per program point and per register class (`pred`, `int32`, `int64`, `float32`, `float64`).
> 4. **Linear Scan Allocator**: Implements Poletto-Sarkar: intervals sorted by start point, active set sorted by end point. When the physical register budget is exceeded, it evicts the candidate with the furthest end-point and assigns local memory spill slots.
> 5. **Graph Coloring Allocator**: Implements Chaitin-Briggs: builds an interference graph from overlapping live intervals, weights spill costs by loop depth ($10^{\text{depth}} / \text{degree}$), applies Simplify and optimistic Spill phases, and assigns lowest available physical registers during Select.
> 6. **Verification & Reporting**: An automated post-allocation verifier mathematically checks the 4 core invariants (no interference overlap, register class consistency, completeness, and budget limits). The report generator outputs text, JSON, DOT graphs, and comparison tables against `ptxas`."

---

## 3. The 5 Hardest Bugs & Design Decisions

### 1. PTX Register Declaration vs. Operand Naming Discrepancies
- **Problem**: PTX permits both range declarations (`.reg .b32 %r<10>;`) and comma-separated discrete declarations (`.reg .b32 %r0, %r1, %r2;` or `.reg .b32 %val;`). Initially, the lexer/parser separated trailing digits for ranges, causing discrete declarations named `"%r0"` to fail lookup against `"%r"` with index 0.
- **Fix**: Replaced split prefix/index storage with a unified full-name string mapping (`regMap_`). Both range expansion and discrete registers register their canonical full names, allowing seamless resolution of user registers, arrays, and hardware registers (`%tid.x`, `%ctaid.x`).

### 2. Declaration Order Independence in Kernel Bodies
- **Problem**: In CUDA PTX output, `.shared`, `.local`, and `.align` directives can appear before, after, or intermixed with `.reg` declarations. A strict loop parsing `.reg` first would stop prematurely on `.shared` and fail with syntax errors on subsequent `.reg` declarations.
- **Fix**: Implemented a unified declaration loop in `Parser::parseKernel()` accepting `.reg`, `.shared`, `.local`, and `.align` in any order.

### 3. Immediate Address Literals inside Brackets
- **Problem**: Shared memory operations such as `ld.shared.u32 %r11, [0];` supply an immediate offset directly inside brackets without a base register. The address expression parser initially expected only identifier or register bases.
- **Fix**: Extended `Parser::parseAddrExpr()` to accept numeric integer literals inside `[...]`, initializing base to `"0"` and recording the offset.

### 4. Cooper-Harvey-Kennedy Predecessor Traversal with Dead Blocks
- **Problem**: When computing dominators using RPO indices, dead or unreachable blocks lack predecessors or valid RPO indices, causing negative indexing during `intersect()`.
- **Fix**: Added explicit guard checking `idom[p] != UNDEFINED` and ensuring all traversed nodes have valid non-negative RPO indices before invoking `intersect()`.

### 5. Loop Weighting Overflow in Chaitin-Briggs
- **Problem**: Computing $10^{\text{depth}}$ for deeply nested loops (e.g. depth > 9) causes integer overflow in 32-bit registers.
- **Fix**: Clamped the loop depth exponent to $\min(\text{depth}, 6)$ and accumulated spill weights in `double`, preventing numeric overflow while maintaining exponential loop cost separation.

---

## 4. Likely Technical Interview Questions & Answers

### Q1: Why does PTX use infinite virtual registers instead of allocating physical registers directly?
**Answer**:
PTX (Parallel Thread Execution) is an intermediate virtual instruction set architecture, not target machine code (SASS). NVIDIA GPUs evolve physical register file sizes, warps, and microarchitectures across compute capabilities (e.g. Kepler to Ampere to Hopper). If PTX targeted a fixed physical register budget, forward compatibility and microarchitectural portability would be broken. Instead, `ptxas` lowers PTX to SASS at compile time (or JIT at driver load time), balancing register allocation against **warp occupancy** (fewer registers per thread allows more concurrent active warps on each SM).

### Q2: What is the fundamental difference between SSA-based register allocation and non-SSA allocation?
**Answer**:
In SSA form, each variable is defined exactly once, which guarantees that live ranges are trees without holes and the interference graph of an SSA program is a **chordal graph**. Chordal graphs can be optimally colored in polynomial time ($O(|V| + |E|)$) using simplicial elimination orderings without backtracking or NP-complete graph coloring. However, real register allocators must handle $\phi$-node elimination, which requires critical edge splitting and register copies that re-introduce non-chordal interferences. Non-SSA allocators (like classic Chaitin-Briggs) operate on arbitrary control-flow graphs and solve the general graph coloring problem with heuristics.

### Q3: What does `ptxas` do differently from traditional CPU register allocators?
**Answer**:
1. **Occupancy Trade-offs**: On CPUs, fewer registers usually mean spilling to L1/stack cache. On GPUs, register count directly determines **SM thread occupancy**. If a kernel uses 33 registers instead of 32, maximum active warps per SM might drop by 50%, devastating memory latency hiding.
2. **Instruction Latency Hiding vs. Register Lifetime**: `ptxas` often deliberately schedules instructions far apart to hide memory latencies, which *lengthens* live intervals and increases register pressure, creating a multi-objective optimization problem between instruction scheduling and register allocation.
3. **Register Bank Conflicts**: NVIDIA GPUs partition register files into physical banks (e.g. 4 banks). Simultaneous operand reads from the same bank incur pipeline stall cycles. `ptxas` allocates physical registers to minimize bank conflicts.

### Q4: Explain the Chaitin-Briggs heuristic for spilling and how optimistic coloring works.
**Answer**:
Chaitin's original algorithm was pessimistic: if every node in the interference graph has degree $\ge K$, it picked a node with minimum spill cost $\text{cost}(u) / \text{deg}(u)$ and permanently spilled it.
Briggs observed that many nodes with degree $\ge K$ can actually be colored because their neighbors may end up sharing colors. In **Chaitin-Briggs**, the spill candidate is pushed onto the stack anyway (**optimistic coloring**). During the Select phase, when popping nodes, we check if an unused color exists. If two neighbors received the same color, an available color remains, and the node is colored without spilling! A spill only occurs if, upon popping, all $K$ colors are genuinely occupied by distinct neighbors.

### Q5: What is Live-Range Splitting, and why is it important in register allocation?
**Answer**:
A variable may have a long live interval where it is only actively used in small bursts at the beginning and end, with a long loop in the middle. Without splitting, that variable occupies a register across the entire loop or interferes with all loop variables, forcing an eviction. **Live-range splitting** breaks the interval into smaller sub-intervals connected by register-to-register moves or spill-load pairs. This allows the variable to reside in a register before the loop, spill into memory or a different register during the loop, and reload afterward.

### Q6: What is Register Coalescing, and what are the risks (e.g. Briggs vs. George criteria)?
**Answer**:
Coalescing eliminates copy instructions (`mov %r1, %r0`) by assigning both virtual registers to the same physical register, merging their nodes in the interference graph.
The risk is **conservative coalescing**: merging two nodes can create a node with high degree that cannot be colored, inducing an expensive spill that costs more than the eliminated copy.
- **Briggs criterion**: Merge $u$ and $v$ only if the merged node has fewer than $K$ neighbors of significant degree ($\ge K$).
- **George criterion**: Merge $u$ and $v$ only if every neighbor of $u$ either already interferes with $v$ or has degree $< K$.
Both criteria guarantee that coalescing does not turn a colorable graph into a non-colorable one.
