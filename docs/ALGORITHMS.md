# PTXRA Algorithm Explanations & Worked Examples

This document details the three core algorithms implemented in PTXRA:
1. **Iterative Backward Liveness Analysis**
2. **Linear Scan Register Allocation (Poletto & Sarkar)**
3. **Graph Coloring Register Allocation (Chaitin-Briggs)**

---

## 1. Iterative Backward Liveness Analysis

### Theory & Equations
Liveness analysis computes, for every program point, which virtual registers may be read before their next definition.

For each basic block $B$:
- **$\text{use}[B]$**: Upward-exposed uses: variables read in $B$ before any definition in $B$.
- **$\text{def}[B]$**: Variables defined (written) in $B$.
- **$\text{liveOut}[B]$**: Variables live immediately after $B$ exits:
  $$\text{liveOut}[B] = \bigcup_{S \in \text{succ}(B)} \text{liveIn}[S]$$
- **$\text{liveIn}[B]$**: Variables live immediately before $B$ enters:
  $$\text{liveIn}[B] = \text{use}[B] \cup (\text{liveOut}[B] \setminus \text{def}[B])$$

The analysis iterates in reverse Reverse Post-Order (RPO) until the $\text{liveIn}$ and $\text{liveOut}$ sets reach a fixed point.

### Worked Example
Consider a function with three basic blocks:
```
BB0 (Entry):
  1: def v0
  2: def v1
  3: branch if cond to BB2 else BB1

BB1:
  4: def v2 = v0 + 10
  5: jump BB2

BB2:
  6: def v3 = v1 + v2
  7: return v3
```

1. **Def / Use Sets**:
   - $\text{BB0}: \text{def}=\{v0, v1\}, \text{use}=\emptyset$
   - $\text{BB1}: \text{def}=\{v2\}, \text{use}=\{v0\}$
   - $\text{BB2}: \text{def}=\{v3\}, \text{use}=\{v1, v2\}$

2. **Iteration 1 (backward from BB2 to BB0)**:
   - $\text{BB2}$:
     - $\text{succ}=\emptyset \implies \text{liveOut}[\text{BB2}] = \emptyset$
     - $\text{liveIn}[\text{BB2}] = \{v1, v2\} \cup (\emptyset \setminus \{v3\}) = \{v1, v2\}$
   - $\text{BB1}$:
     - $\text{succ}=\{\text{BB2}\} \implies \text{liveOut}[\text{BB1}] = \{v1, v2\}$
     - $\text{liveIn}[\text{BB1}] = \{v0\} \cup (\{v1, v2\} \setminus \{v2\}) = \{v0, v1\}$
   - $\text{BB0}$:
     - $\text{succ}=\{\text{BB1}, \text{BB2}\} \implies \text{liveOut}[\text{BB0}] = \text{liveIn}[\text{BB1}] \cup \text{liveIn}[\text{BB2}] = \{v0, v1\} \cup \{v1, v2\} = \{v0, v1, v2\}$
     - $\text{liveIn}[\text{BB0}] = \emptyset \cup (\{v0, v1, v2\} \setminus \{v0, v1\}) = \{v2\}$ (uninitialized in BB0 path if BB2 taken directly; caught by verifier/warning).

3. Fixed point reached in 2 iterations.

---

## 2. Linear Scan Register Allocation (Poletto & Sarkar)

### Algorithm Steps
1. Sort all live intervals by their `start` point in ascending order.
2. Maintain an `active` set of currently allocated intervals, sorted by `end` point.
3. For each incoming interval $I$:
   a. **Expire Old Intervals**: Remove all intervals in `active` where $end < I.start$, returning their physical registers to the free pool.
   b. **Allocate**: If a free register is available, assign the lowest-indexed register to $I$ and insert $I$ into `active`.
   c. **Spill at Interval**: If no register is free:
      - Compare the interval $A$ in `active` with the largest `end` point against $I$.
      - If $A.end > I.end$: evict $A$, assign $A$'s physical register to $I$, and spill $A$ to local memory.
      - Else: spill $I$ directly.

### Worked Example
Suppose physical register limit $K = 2$, and we have 3 intervals:
- $I_A: [1, 10]$
- $I_B: [2, 6]$
- $I_C: [4, 8]$

**Trace**:
1. **$t = 1$, process $I_A [1, 10]$**:
   - `active` is empty. Free registers: $\{R_0, R_1\}$.
   - Assign $I_A \to R_0$. `active` $= \{I_A [1, 10]\}$.
2. **$t = 2$, process $I_B [2, 6]$**:
   - Expire: None ($10 \ge 2$).
   - Free registers: $\{R_1\}$.
   - Assign $I_B \to R_1$. `active` $= \{I_B [2, 6], I_A [1, 10]\}$.
3. **$t = 4$, process $I_C [4, 8]$**:
   - Expire: None ($6 \ge 4, 10 \ge 4$).
   - Free registers: $\emptyset$. Pressure exceeds limit $K=2$.
   - Largest end in `active` is $I_A$ ($end = 10$).
   - Since $I_A.end (10) > I_C.end (8)$:
     - **Spill $I_A$** to local memory slot 0.
     - Reassign $R_0$ to $I_C$.
     - `active` $= \{I_B [2, 6], I_C [4, 8]\}$.
4. **$t = 6$**: $I_B$ expires. $R_1$ freed.
5. **$t = 8$**: $I_C$ expires. $R_0$ freed.

**Final Result**:
- $I_B \to R_1$, $I_C \to R_0$, $I_A \to \text{spill slot 0}$.

---

## 3. Graph Coloring Register Allocation (Chaitin-Briggs)

### Algorithm Steps
1. **Build**: Construct the interference graph $G = (V, E)$, where each vertex is an active virtual register and an edge $(u, v)$ indicates $u$ and $v$ have overlapping live ranges.
2. **Compute Spill Cost**: For each node $v$, compute:
   $$\text{cost}(v) = \frac{\sum_{r \in \text{refs}(v)} 10^{\text{loopDepth}(r)}}{\text{degree}(v)}$$
3. **Simplify**: While there exists a node $v$ with $\text{degree}(v) < K$:
   - Remove $v$ from $G$ and push $v$ onto the select stack.
   - Decrement the degree of all neighbors of $v$.
4. **Optimistic Spill (Briggs)**: If $G$ is non-empty and every node has $\text{degree} \ge K$:
   - Choose node $s$ with the minimum $\text{cost}(s)$.
   - Remove $s$ and push it to the select stack (hoping colors freed later will allow it to be colored).
5. **Select**: Pop nodes from the stack in reverse order:
   - For node $u$, find all colors $\{0 \dots K-1\}$ used by already-colored neighbors.
   - If an unused color $c < K$ exists, assign $u \to c$.
   - If no color is available (an optimistic candidate that failed), mark $u$ as spilled!
6. **Convergence**: If any node spilled, insert spill code (`ld.local`/`st.local`) and iterate until all nodes are colored.

### Worked Example
Suppose $K = 2$ colors, with 4 variables forming a chordless 4-cycle $C_4$ plus an edge:
- Variables: $v_0, v_1, v_2, v_3$
- Edges: $(v_0, v_1), (v_1, v_2), (v_2, v_3), (v_3, v_0)$, and $(v_0, v_2)$.
- Degrees: $\text{deg}(v_0) = 3, \text{deg}(v_1) = 2, \text{deg}(v_2) = 3, \text{deg}(v_3) = 2$.
- Loop depths: $v_0$ is outside loop (cost = 1/3), $v_2$ is inside loop (cost = 100/3).

**Execution**:
1. All nodes have $\text{degree} \ge 2$, so no node has $\text{degree} < 2$.
2. **Spill Phase**:
   - Compare candidates: $v_0$ has lowest cost (1/3).
   - Push $v_0$ onto stack.
   - Remaining graph on $\{v_1, v_2, v_3\}$ has degrees:
     $\text{deg}(v_1) = 1, \text{deg}(v_2) = 2, \text{deg}(v_3) = 1$.
3. **Simplify Phase**:
   - $v_1$ has $\text{degree} = 1 < 2 \implies$ push $v_1$.
   - $v_3$ has $\text{degree} = 1 < 2 \implies$ push $v_3$.
   - $v_2$ now has $\text{degree} = 0 < 2 \implies$ push $v_2$.
4. **Stack (top to bottom)**: $[v_2, v_3, v_1, v_0]$.
5. **Select Phase**:
   - Pop $v_2$: Neighbors colored = $\emptyset \implies$ Assign Color 0 ($R_0$).
   - Pop $v_3$: Neighbor $v_2$ has Color 0 $\implies$ Assign Color 1 ($R_1$).
   - Pop $v_1$: Neighbor $v_2$ has Color 0 $\implies$ Assign Color 1 ($R_1$).
   - Pop $v_0$: Neighbors $v_1$ (Color 1), $v_2$ (Color 0), $v_3$ (Color 1).
     Both Color 0 and Color 1 are taken!
     $v_0$ cannot be colored $\implies$ **Spill $v_0$** to local memory slot 0.

**Final Result**:
- $v_1 \to R_1$, $v_2 \to R_0$, $v_3 \to R_1$, $v_0 \to \text{spill slot 0}$.
- Loop variable $v_2$ remained in physical register, protecting inner-loop performance.
