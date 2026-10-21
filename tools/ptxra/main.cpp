#include "../../src/lexer/Lexer.h"
#include "../../src/parser/Parser.h"
#include "../../src/ast/ASTPrinter.h"
#include "../../src/cfg/CFGBuilder.h"
#include "../../src/cfg/Dominator.h"
#include "../../src/cfg/LoopInfo.h"
#include "../../src/cfg/DOTEmitter.h"
#include "../../src/analysis/Liveness.h"
#include "../../src/analysis/LiveInterval.h"
#include "../../src/analysis/Pressure.h"
#include "../../src/regalloc/LinearScan.h"
#include "../../src/regalloc/GraphColor.h"
#include "../../src/regalloc/Verifier.h"
#include "../../src/report/TextReport.h"
#include "../../src/report/JSONReport.h"
#include "../../src/report/CSVReport.h"
#include "../../src/support/StringUtil.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>

using namespace ptxra;

static void printUsage() {
    std::cerr << "Usage: ptxra <command> <file.ptx> [options]\n"
              << "Commands:\n"
              << "  parse    <file.ptx> [--print] [--json]\n"
              << "  cfg      <file.ptx> [--dot] [--json]\n"
              << "  liveness <file.ptx> [--json]\n"
              << "  pressure <file.ptx> [--json]\n"
              << "  alloc    <file.ptx> [--algo=linear|graph] [--max-regs=N] [--json]\n"
              << "  compare  <file.ptx> [--max-regs=N] [--ptxas] [--csv] [--json]\n"
              << "Options:\n"
              << "  --max-regs=N   Physical register limit per class (default: 32)\n"
              << "  --algo=NAME    Allocator algorithm: linear or graph (default: linear)\n"
              << "  --dot          Output Graphviz DOT for CFG\n"
              << "  --json         Output JSON format\n"
              << "  --csv          Output CSV format (for compare)\n"
              << "  --print        Pretty-print AST (for parse)\n"
              << "  --ptxas        Compare with ptxas -v output if available\n";
}

static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open file '" << path << "'\n";
        std::exit(1);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static ast::Module parsePTX(const std::string& path, const std::string& src) {
    Lexer lexer(src, path);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens), path);
    return parser.parse();
}

static bool runPtxas(const std::string& ptxPath, int& regs, int& spillSt, int& spillLd, int& stackBytes) {
    std::string cmd = "ptxas -v -arch=sm_80 \"" + ptxPath + "\" -o /dev/null 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buffer[256];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    int code = pclose(pipe);
    if (code != 0) return false;

    // Parse ptxas output: e.g. "ptxas info    : Used 10 registers, 0 bytes smem"
    // "ptxas info    : 0 bytes spill stores, 0 bytes spill loads"
    // "ptxas info    : 0 bytes stack frame"
    auto parseVal = [&](const std::string& key) -> int {
        auto pos = output.find(key);
        if (pos == std::string::npos) return 0;
        // Walk backward to find number
        int end = static_cast<int>(pos) - 1;
        while (end >= 0 && output[static_cast<std::size_t>(end)] == ' ') --end;
        int start = end;
        while (start >= 0 && std::isdigit(static_cast<unsigned char>(output[static_cast<std::size_t>(start)]))) --start;
        if (start < end) {
            try {
                return std::stoi(output.substr(static_cast<std::size_t>(start + 1), static_cast<std::size_t>(end - start)));
            } catch (...) {}
        }
        return 0;
    };

    regs = parseVal("registers");
    spillSt = parseVal("bytes spill stores");
    spillLd = parseVal("bytes spill loads");
    stackBytes = parseVal("bytes stack frame");
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];
    std::string ptxPath = argv[2];

    int maxRegs = 255;
    std::string algo = "linear";
    bool optDot = false;
    bool optJson = false;
    bool optCsv = false;
    bool optPrint = false;
    bool optPtxas = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (startsWith(arg, "--max-regs=")) {
            maxRegs = std::stoi(arg.substr(11));
        } else if (startsWith(arg, "--algo=")) {
            algo = arg.substr(7);
        } else if (arg == "--dot") {
            optDot = true;
        } else if (arg == "--json") {
            optJson = true;
        } else if (arg == "--csv") {
            optCsv = true;
        } else if (arg == "--print") {
            optPrint = true;
        } else if (arg == "--ptxas") {
            optPtxas = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    std::string source = readFile(ptxPath);
    ast::Module mod;
    try {
        mod = parsePTX(ptxPath, source);
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << "\n";
        return 1;
    }

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(maxRegs);

    if (cmd == "parse") {
        if (optPrint) {
            ast::ASTPrinter printer(std::cout);
            printer.print(mod);
        } else if (optJson) {
            std::cout << "{\n  \"version\": \"" << mod.version << "\",\n"
                      << "  \"target\": \"" << mod.target << "\",\n"
                      << "  \"address_size\": " << mod.addressSize << ",\n"
                      << "  \"num_kernels\": " << mod.kernels.size() << "\n}\n";
        } else {
            std::cout << "Successfully parsed " << ptxPath << ":\n"
                      << "  Version:      " << mod.version << "\n"
                      << "  Target:       " << mod.target << "\n"
                      << "  Address Size: " << mod.addressSize << "\n"
                      << "  Kernels:      " << mod.kernels.size() << "\n";
            for (const auto& k : mod.kernels) {
                std::cout << "    - " << k.name << " (" << k.body.size() << " statements, "
                          << k.regDecls.size() << " reg decls)\n";
            }
        }
        return 0;
    }

    cfg::CFGBuilder cfgBuilder;
    analysis::LivenessAnalysis livenessAnalysis;

    if (cmd == "cfg") {
        for (const auto& k : mod.kernels) {
            auto fn = cfgBuilder.build(k, ptxPath);
            if (optDot) {
                cfg::DOTEmitter dot;
                dot.emit(*fn, std::cout);
            } else if (optJson) {
                report::JSONReport::emitFunction(*fn, nullptr, nullptr, nullptr, nullptr, std::cout);
            } else {
                report::TextReport::printFunctionCFG(*fn, std::cout);
            }
        }
        return 0;
    }

    if (cmd == "liveness") {
        for (const auto& k : mod.kernels) {
            auto fn = cfgBuilder.build(k, ptxPath);
            auto live = livenessAnalysis.compute(*fn);
            if (optJson) {
                report::JSONReport::emitFunction(*fn, &live, nullptr, nullptr, nullptr, std::cout);
            } else {
                report::TextReport::printLiveness(*fn, live, std::cout);
            }
        }
        return 0;
    }

    if (cmd == "pressure") {
        for (const auto& k : mod.kernels) {
            auto fn = cfgBuilder.build(k, ptxPath);
            auto live = livenessAnalysis.compute(*fn);
            auto intervals = analysis::LiveIntervals::compute(*fn, live);
            auto press = analysis::PressureResult::compute(*fn, intervals);
            if (optJson) {
                report::JSONReport::emitFunction(*fn, &live, &press, nullptr, nullptr, std::cout);
            } else {
                report::TextReport::printPressure(*fn, press, std::cout);
            }
        }
        return 0;
    }

    if (cmd == "alloc") {
        bool allPassed = true;
        for (const auto& k : mod.kernels) {
            auto fn = cfgBuilder.build(k, ptxPath);
            cfg::DominatorTree dom;
            dom.compute(*fn);
            cfg::LoopInfo loops;
            loops.compute(*fn, dom);

            auto live = livenessAnalysis.compute(*fn);
            auto intervals = analysis::LiveIntervals::compute(*fn, live);
            auto press = analysis::PressureResult::compute(*fn, intervals);

            regalloc::AllocationMap alloc;
            regalloc::AllocStats stats;

            if (algo == "graph") {
                regalloc::GraphColorAllocator gc(limits);
                alloc = gc.allocate(*fn, intervals, loops, stats, &live);
            } else {
                regalloc::LinearScanAllocator ls(limits);
                alloc = ls.allocate(*fn, intervals, stats, &loops);
            }

            // Run verification pass
            auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
            if (!vres.passed) {
                std::cerr << "Verification FAILED for kernel '" << fn->name << "':\n";
                for (const auto& err : vres.errors) {
                    std::cerr << "  - " << err << "\n";
                }
                allPassed = false;
            }

            if (optJson) {
                report::JSONReport::emitFunction(*fn, &live, &press, &stats, &alloc, std::cout);
            } else {
                report::TextReport::printAllocation(*fn, stats, alloc, std::cout);
                if (vres.passed) {
                    std::cout << "  Verification: PASSED (all allocation invariants satisfied)\n\n";
                }
            }
        }
        return allPassed ? 0 : 2;
    }

    if (cmd == "compare") {
        std::vector<report::KernelComparisonRow> rows;

        for (const auto& k : mod.kernels) {
            report::KernelComparisonRow row;
            row.kernelName = k.name;
            row.budget = maxRegs;

            // 1. Linear scan
            {
                auto fn = cfgBuilder.build(k, ptxPath);
                cfg::DominatorTree dom;
                dom.compute(*fn);
                cfg::LoopInfo loops;
                loops.compute(*fn, dom);

                auto live = livenessAnalysis.compute(*fn);
                auto intervals = analysis::LiveIntervals::compute(*fn, live);
                regalloc::LinearScanAllocator ls(limits);
                regalloc::AllocStats stats;
                auto alloc = ls.allocate(*fn, intervals, stats, &loops);

                auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
                if (!vres.passed) {
                    std::cerr << "Linear scan verification failed for " << fn->name << "\n";
                }
                row.lsRegs = stats.registersUsed;
                row.lsPreds = stats.predicatesUsed;
                row.lsSpillStores = stats.spillStores;
                row.lsSpillLoads = stats.spillLoads;
                row.lsSpillCost = stats.spillCost;
            }

            // 2. Graph coloring
            {
                auto fn = cfgBuilder.build(k, ptxPath);
                cfg::DominatorTree dom;
                dom.compute(*fn);
                cfg::LoopInfo loops;
                loops.compute(*fn, dom);

                auto live = livenessAnalysis.compute(*fn);
                auto intervals = analysis::LiveIntervals::compute(*fn, live);
                regalloc::GraphColorAllocator gc(limits);
                regalloc::AllocStats stats;
                auto alloc = gc.allocate(*fn, intervals, loops, stats, &live);

                auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
                if (!vres.passed) {
                    std::cerr << "Graph color verification failed for " << fn->name << "\n";
                }
                row.gcRegs = stats.registersUsed;
                row.gcPreds = stats.predicatesUsed;
                row.gcSpillStores = stats.spillStores;
                row.gcSpillLoads = stats.spillLoads;
                row.gcSpillCost = stats.spillCost;
            }

            // 3. Optional ptxas
            if (optPtxas) {
                int pRegs = 0, pSt = 0, pLd = 0, pStack = 0;
                if (runPtxas(ptxPath, pRegs, pSt, pLd, pStack)) {
                    row.hasPtxas = true;
                    row.ptxasRegs = pRegs;
                    row.ptxasSpillStores = pSt;
                    row.ptxasSpillLoads = pLd;
                    row.ptxasStackFrame = pStack;
                }
            }

            rows.push_back(row);
        }

        if (optCsv) {
            report::ComparisonReporter::emitCSV(rows, std::cout);
        } else {
            report::ComparisonReporter::emitMarkdown(rows, std::cout);
            if (optPtxas && !rows.empty() && !rows.front().hasPtxas) {
                std::cout << "\n*Note: ptxas not found or invocation failed; PTXAS columns marked N/A.*\n";
            }
        }
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    printUsage();
    return 1;
}
