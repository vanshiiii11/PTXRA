#include "TextReport.h"
#include <iomanip>

namespace ptxra {
namespace report {

void TextReport::printFunctionCFG(const ir::Function& fn, std::ostream& os) {
    os << "Function: " << fn.name << " (" << (fn.isEntry ? "kernel" : "device func") << ")\n";
    os << "  Blocks: " << fn.blocks.size() << "\n";
    os << "  Virtual Registers: " << fn.numVRegs() << "\n\n";

    for (const auto& blk : fn.blocks) {
        os << "  Block BB" << blk->id;
        if (!blk->label.empty()) os << " (" << blk->label << ")";
        os << " [depth=" << blk->loopDepth << "]:\n";

        os << "    Predecessors: ";
        for (std::size_t i = 0; i < blk->preds.size(); ++i) {
            if (i > 0) os << ", ";
            os << "BB" << blk->preds[i];
        }
        os << "\n";

        os << "    Successors:   ";
        for (std::size_t i = 0; i < blk->succs.size(); ++i) {
            if (i > 0) os << ", ";
            os << "BB" << blk->succs[i];
        }
        os << "\n";

        os << "    Instructions: " << blk->insns.size() << "\n";
        for (const auto& insn : blk->insns) {
            os << "      " << insn.toString() << "\n";
        }
        os << "\n";
    }
}

void TextReport::printLiveness(const ir::Function& fn, const analysis::LivenessResult& live, std::ostream& os) {
    os << "Liveness Analysis for " << fn.name << ":\n";
    for (const auto& blk : fn.blocks) {
        const auto& bl = live.forBlock(blk->id);
        os << "  BB" << blk->id;
        if (!blk->label.empty()) os << " (" << blk->label << ")";
        os << ":\n";

        auto printSet = [&](const char* name, const analysis::LiveSet& s) {
            os << "    " << std::left << std::setw(10) << name << ": { ";
            bool first = true;
            for (auto vid : s) {
                if (!first) os << ", ";
                os << "v" << vid << " (" << fn.vreg(vid).name << ")";
                first = false;
            }
            os << " }\n";
        };

        printSet("USE", bl.use);
        printSet("DEF", bl.def);
        printSet("LIVE-IN", bl.liveIn);
        printSet("LIVE-OUT", bl.liveOut);
    }
}

void TextReport::printPressure(const ir::Function& fn, const analysis::PressureResult& press, std::ostream& os) {
    os << "Register Pressure for " << fn.name << ":\n";
    os << "  Peak Total Pressure:  " << press.peak.total() << " vregs (at program point "
       << press.peak.programPoint << ")\n";
    os << "  Peak by Class:\n";
    os << "    Pred:    " << press.peakPred << "\n";
    os << "    Int32:   " << press.peakInt32 << "\n";
    os << "    Int64:   " << press.peakInt64 << "\n";
    os << "    Float32: " << press.peakFloat32 << "\n";
    os << "    Float64: " << press.peakFloat64 << "\n";
}

void TextReport::printAllocation(const ir::Function& fn, const regalloc::AllocStats& stats,
                                const regalloc::AllocationMap& alloc, std::ostream& os) {
    os << "Allocation Results (" << stats.allocatorName << ") for " << fn.name << ":\n";
    os << "  Registers Used:  " << stats.registersUsed << " (highest 32-bit slot + 1)\n";
    os << "  Predicates Used: " << stats.predicatesUsed << "\n";
    if (stats.scratchSlots > 0) {
        os << "  Scratch Slots:   " << stats.scratchSlots << " reserved\n";
    }
    os << "  Spills:\n";
    os << "    Spill Stores:  " << stats.spillStores << "\n";
    os << "    Spill Loads:   " << stats.spillLoads << "\n";
    os << "    Spill Cost:    " << static_cast<long long>(stats.spillCost) << " (loop-weighted)\n";
    os << "    Spill Slots:   " << alloc.spillSlot.size() << "\n";
}

} // namespace report
} // namespace ptxra
