import os
import subprocess
import json

kernels = [
    "divergent",
    "heavy_pressure",
    "matmul",
    "reduction",
    "saxpy",
    "stencil",
    "vector_add",
    "nested_loops",
    "long_lived",
    "live_range_hole",
    "fma_chain"
]

budgets = [16, 24, 32, 64, 255]
allocators = [("linear", "LinearScan"), ("graph", "GraphColoring")]

rows = []

for k in kernels:
    ptx_file = f"samples/{k}.ptx"
    for b in budgets:
        for algo_flag, algo_name in allocators:
            cmd = ["./build/ptxra", "alloc", ptx_file, f"--algo={algo_flag}", f"--max-regs={b}", "--json"]
            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode != 0:
                print(f"Error running {cmd}: {res.stderr}")
                continue
            data = json.loads(res.stdout)
            alloc = data.get("allocation", {})
            kname = data.get("kernel", k)
            slots_used = alloc.get("registers_used", 0)
            spill_st = alloc.get("spill_stores", 0)
            spill_ld = alloc.get("spill_loads", 0)
            spill_cost = alloc.get("spill_cost", 0.0)

            rows.append({
                "kernel": kname,
                "budget": b,
                "allocator": algo_name,
                "slots_used": slots_used,
                "spill_stores": spill_st,
                "spill_loads": spill_ld,
                "weighted_spill_cost": int(spill_cost)
            })

# Output Markdown table
md_lines = []
md_lines.append("| kernel | budget | allocator | slots used | spill stores | spill loads | weighted spill cost |")
md_lines.append("| :--- | :---: | :---: | :---: | :---: | :---: | :---: |")
for r in rows:
    md_lines.append(f"| `{r['kernel']}` | {r['budget']} | {r['allocator']} | {r['slots_used']} | {r['spill_stores']} | {r['spill_loads']} | {r['weighted_spill_cost']} |")

# Output CSV
csv_lines = []
csv_lines.append("kernel,budget,allocator,slots used,spill stores,spill loads,weighted spill cost")
for r in rows:
    csv_lines.append(f"{r['kernel']},{r['budget']},{r['allocator']},{r['slots_used']},{r['spill_stores']},{r['spill_loads']},{r['weighted_spill_cost']}")

with open("benchmark_results.md", "w") as f:
    f.write("\n".join(md_lines) + "\n")

with open("benchmark_results.csv", "w") as f:
    f.write("\n".join(csv_lines) + "\n")

print("Generated benchmark_results.md and benchmark_results.csv successfully.")
