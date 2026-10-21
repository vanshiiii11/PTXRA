import os
import subprocess
import re
from collections import defaultdict

root_dir = os.path.abspath(".")
build_cov = os.path.join(root_dir, "build-cov")

component_map = {
    "lexer": "src/lexer",
    "parser": "src/parser",
    "ast": "src/ast",
    "ir": "src/ir",
    "cfg": "src/cfg",
    "analysis": "src/analysis",
    "regalloc": "src/regalloc",
    "report": "src/report"
}

# Run gcov on each gcda file
cov_dir = os.path.join(build_cov, "gcov_out")
os.makedirs(cov_dir, exist_ok=True)

gcda_files = []
for root, dirs, files in os.walk(os.path.join(build_cov, "CMakeFiles", "ptxra_core.dir")):
    for f in files:
        if f.endswith(".gcda"):
            gcda_files.append(os.path.join(root, f))

file_stats = {} # filepath -> (executed, total)

for gcda in gcda_files:
    obj_dir = os.path.dirname(gcda)
    res = subprocess.run(["gcov", "-o", obj_dir, gcda], cwd=cov_dir, capture_output=True, text=True)
    lines = res.stdout.splitlines()
    curr_file = None
    for line in lines:
        if line.startswith("File '"):
            curr_file = line[6:-1]
        elif line.startswith("Lines executed:") and curr_file:
            m = re.match(r"Lines executed:([\d\.]+)% of (\d+)", line)
            if m:
                pct = float(m.group(1))
                total = int(m.group(2))
                executed = int(round(pct * total / 100.0))
                # Only keep project src files
                if "/src/" in curr_file and not curr_file.startswith("/Library"):
                    file_stats[curr_file] = (executed, total)
            curr_file = None

comp_stats = defaultdict(lambda: [0, 0])
for path, (exec_lines, tot_lines) in file_stats.items():
    for comp, prefix in component_map.items():
        if prefix in path:
            comp_stats[comp][0] += exec_lines
            comp_stats[comp][1] += tot_lines
            break

print(f"| Component | Executed Lines | Total Lines | Coverage % |")
print(f"| :--- | :---: | :---: | :---: |")
tot_e = 0
tot_t = 0
for comp in ["lexer", "parser", "ast", "ir", "cfg", "analysis", "regalloc", "report"]:
    e, t = comp_stats[comp]
    pct = (e / t * 100.0) if t > 0 else 0.0
    tot_e += e
    tot_t += t
    print(f"| `{comp}` | {e} | {t} | {pct:.1f}% |")

tot_pct = (tot_e / tot_t * 100.0) if tot_t > 0 else 0.0
print(f"| **Total** | **{tot_e}** | **{tot_t}** | **{tot_pct:.1f}%** |")
