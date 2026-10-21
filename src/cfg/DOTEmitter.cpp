#include "DOTEmitter.h"
#include "../ir/Instruction.h"
#include <sstream>
#include <algorithm>

// Pull in the full LivenessResult definition only if the header exists;
// since it's not written yet, we guard by only using the pointer as opaque.
// When Liveness.h is added, include it here and implement the annotation path.
// #include "../analysis/Liveness.h"

namespace ptxra {
namespace cfg {

// ---------------------------------------------------------------------------
// escape — replace DOT special characters so the string is safe inside a
// record-label field.  Characters that need escaping in DOT record labels:
//   { } | < > " \  and  newline (replaced with \l for left-justified lines).
// ---------------------------------------------------------------------------
std::string DOTEmitter::escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '{':  out += "\\{"; break;
            case '}':  out += "\\}"; break;
            case '|':  out += "\\|"; break;
            case '<':  out += "\\<"; break;
            case '>':  out += "\\>"; break;
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\l"; break;  // left-justify in DOT records
            default:   out += c;   break;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// blockLabel — build a DOT record label string for one basic block.
//
// Format:
//   {BB<id>: <label>|insn0\linsn1\l...|live-in: ...\llive-out: ...\l}
//
// The live-in/live-out section is only appended when liveness != nullptr
// and the block's info can be queried.  Because LivenessResult is forward-
// declared (not yet defined), we leave the annotation path as a structured
// comment stub that compiles cleanly today and integrates trivially once
// Liveness.h is written.
// ---------------------------------------------------------------------------
std::string DOTEmitter::blockLabel(const ir::BasicBlock& blk,
                                   const ir::Function&   fn,
                                   const analysis::LivenessResult* liveness) {
    std::ostringstream oss;

    // ---- Header row: block id and optional PTX label ----
    oss << "BB" << blk.id;
    if (!blk.label.empty())
        oss << ": " << escape(blk.label);

    // ---- Instruction rows ----
    std::string insns;
    for (const ir::Instruction& insn : blk.insns) {
        insns += escape(insn.toString()) + "\\l";
    }

    // ---- Build record: { header | insns } ----
    // Optionally add liveness rows.
    std::string result;
    result += "{";
    result += oss.str();         // header
    if (!insns.empty()) {
        result += "|";
        result += insns;         // instructions
    }

    // Liveness annotation — integrated once analysis::LivenessResult is defined.
    // When that happens, replace this block with:
    //
    //   if (liveness) {
    //       auto& info = liveness->blockInfo(blk.id);
    //       std::string liveIn  = formatVRegSet(info.liveIn,  fn);
    //       std::string liveOut = formatVRegSet(info.liveOut, fn);
    //       result += "|live-in: "  + liveIn  + "\\l";
    //       result += "live-out: " + liveOut + "\\l";
    //   }
    //
    (void)liveness; // suppress unused-parameter warning until path is wired in
    (void)fn;       // same

    result += "}";
    return result;
}

// ---------------------------------------------------------------------------
// emit — write a complete DOT graph for the function to `out`.
//
// Output format:
//   digraph "funcname" {
//     node [shape=record];
//     B0 [label="{BB0: __entry__|insn1\linsn2\l}"];
//     B0 -> B1;
//     ...
//   }
// ---------------------------------------------------------------------------
void DOTEmitter::emit(const ir::Function& fn, std::ostream& out,
                      const analysis::LivenessResult* liveness) {
    // Graph header
    out << "digraph \"" << escape(fn.name) << "\" {\n";
    out << "  node [shape=record];\n";
    out << "\n";

    // --- Node declarations (one per basic block) ---
    for (const auto& blkPtr : fn.blocks) {
        const ir::BasicBlock& blk = *blkPtr;

        // Node identifier: B<id>  (avoids collision with DOT keywords)
        std::string nodeId = "B" + std::to_string(blk.id);

        std::string label = blockLabel(blk, fn, liveness);

        out << "  " << nodeId
            << " [label=\"" << label << "\"];\n";
    }

    out << "\n";

    // --- Edge declarations ---
    for (const auto& blkPtr : fn.blocks) {
        const ir::BasicBlock& blk = *blkPtr;
        std::string srcId = "B" + std::to_string(blk.id);

        for (ir::BlockId succId : blk.succs) {
            std::string dstId = "B" + std::to_string(succId);
            out << "  " << srcId << " -> " << dstId << ";\n";
        }
    }

    // Graph footer
    out << "}\n";
}

} // namespace cfg
} // namespace ptxra
