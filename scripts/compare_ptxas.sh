#!/usr/bin/env bash
set -euo pipefail

# Check if nvcc and ptxas are available
if ! command -v nvcc &> /dev/null || ! command -v ptxas &> /dev/null; then
    echo "Warning: nvcc or ptxas not found in PATH."
    echo "This script requires the NVIDIA CUDA Toolkit to re-compile .cu files and run ptxas."
    echo "Skipping live ptxas run. Use pre-generated .ptx files in samples/ instead."
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SAMPLES_DIR="$REPO_ROOT/samples"

echo "================================================================================"
echo "Compiling CUDA samples to PTX and analyzing with ptxas -v -arch=sm_80"
echo "================================================================================"

printf "%-20s | %-10s | %-12s | %-12s | %-12s\n" "Kernel" "Registers" "Spill Stores" "Spill Loads" "Stack Frame"
printf "%s\n" "--------------------------------------------------------------------------------"

for cu_file in "$SAMPLES_DIR"/*.cu; do
    base="$(basename "$cu_file" .cu)"
    ptx_tmp="/tmp/${base}_test.ptx"

    # Compile .cu to .ptx
    nvcc -ptx -arch=sm_80 "$cu_file" -o "$ptx_tmp"

    # Run ptxas -v
    ptxas_out="$(ptxas -v -arch=sm_80 "$ptx_tmp" -o /dev/null 2>&1 || true)"

    # Parse metrics
    regs="$(echo "$ptxas_out" | grep -oE '[0-9]+ registers' | awk '{print $1}' || echo "N/A")"
    spill_st="$(echo "$ptxas_out" | grep -oE '[0-9]+ bytes spill stores' | awk '{print $1}' || echo "0")"
    spill_ld="$(echo "$ptxas_out" | grep -oE '[0-9]+ bytes spill loads' | awk '{print $1}' || echo "0")"
    stack="$(echo "$ptxas_out" | grep -oE '[0-9]+ bytes stack frame' | awk '{print $1}' || echo "0")"

    printf "%-20s | %-10s | %-12s | %-12s | %-12s\n" "$base" "$regs" "${spill_st}B" "${spill_ld}B" "${stack}B"
    rm -f "$ptx_tmp"
done

echo "================================================================================"
