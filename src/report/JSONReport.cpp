#include "JSONReport.h"
#include <sstream>

namespace ptxra {
namespace report {

static std::string escapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

void JSONReport::emitFunction(const ir::Function& fn,
                             const analysis::LivenessResult* live,
                             const analysis::PressureResult* press,
                             const regalloc::AllocStats* stats,
                             const regalloc::AllocationMap* alloc,
                             std::ostream& os) {
    os << "{\n";
    os << "  \"kernel\": \"" << escapeJson(fn.name) << "\",\n";
    os << "  \"is_entry\": " << (fn.isEntry ? "true" : "false") << ",\n";
    os << "  \"num_blocks\": " << fn.blocks.size() << ",\n";
    os << "  \"num_vregs\": " << fn.numVRegs();

    if (press) {
        os << ",\n  \"pressure\": {\n";
        os << "    \"peak_total\": " << press->peak.total() << ",\n";
        os << "    \"peak_program_point\": " << press->peak.programPoint << ",\n";
        os << "    \"peak_pred\": " << press->peakPred << ",\n";
        os << "    \"peak_int32\": " << press->peakInt32 << ",\n";
        os << "    \"peak_int64\": " << press->peakInt64 << ",\n";
        os << "    \"peak_float32\": " << press->peakFloat32 << ",\n";
        os << "    \"peak_float64\": " << press->peakFloat64 << "\n";
        os << "  }";
    }

    if (live) {
        os << ",\n  \"blocks\": [\n";
        for (std::size_t i = 0; i < fn.blocks.size(); ++i) {
            const auto& blk = fn.blocks[i];
            const auto& bl = live->forBlock(blk->id);
            os << "    {\n";
            os << "      \"id\": " << blk->id << ",\n";
            os << "      \"label\": \"" << escapeJson(blk->label) << "\",\n";
            os << "      \"loop_depth\": " << blk->loopDepth << ",\n";
            os << "      \"live_in_count\": " << bl.liveIn.size() << ",\n";
            os << "      \"live_out_count\": " << bl.liveOut.size() << "\n";
            os << "    }" << (i + 1 < fn.blocks.size() ? "," : "") << "\n";
        }
        os << "  ]";
    }

    if (stats && alloc) {
        os << ",\n  \"allocation\": {\n";
        os << "    \"algorithm\": \"" << escapeJson(stats->allocatorName) << "\",\n";
        os << "    \"registers_used\": " << stats->registersUsed << ",\n";
        os << "    \"predicates_used\": " << stats->predicatesUsed << ",\n";
        os << "    \"scratch_slots\": " << stats->scratchSlots << ",\n";
        os << "    \"spill_stores\": " << stats->spillStores << ",\n";
        os << "    \"spill_loads\": " << stats->spillLoads << ",\n";
        os << "    \"spill_cost\": " << stats->spillCost << ",\n";
        os << "    \"spill_slots\": " << alloc->spillSlot.size() << "\n";
        os << "  }";
    }

    os << "\n}\n";
}

} // namespace report
} // namespace ptxra
