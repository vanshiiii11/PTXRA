#include "SpillManager.h"
#include <cassert>

namespace ptxra {
namespace regalloc {

int SpillManager::allocateSlot(ir::VRegId /*vreg*/) {
    return fn_.spillSlotCount++;
}

void SpillManager::rewriteSpills(const AllocationMap& alloc) {
    using namespace ir;

    for (auto& blkPtr : fn_.blocks) {
        std::vector<Instruction> newInsns;
        newInsns.reserve(blkPtr->insns.size() * 2);

        for (auto& insn : blkPtr->insns) {
            // For each use that is spilled, insert a load before the instruction
            bool needLoad = false;
            for (const auto& op : insn.uses) {
                if (op.isReg() && alloc.isSpilled(op.asReg())) {
                    needLoad = true;
                    break;
                }
            }
            if (needLoad) {
                for (const auto& op : insn.uses) {
                    if (op.isReg() && alloc.isSpilled(op.asReg())) {
                        VRegId vid = op.asReg();
                        int slot = alloc.getSpillSlot(vid);
                        Instruction ld;
                        ld.opcode    = IrOpcode::SpillLoad;
                        ld.type      = fn_.vreg(vid).type;
                        ld.spillSlot = slot;
                        ld.defs.push_back(Operand::reg(vid));
                        newInsns.push_back(std::move(ld));
                    }
                }
            }

            newInsns.push_back(insn);

            // For each def that is spilled, insert a store after the instruction
            for (const auto& op : insn.defs) {
                if (op.isReg() && alloc.isSpilled(op.asReg())) {
                    VRegId vid = op.asReg();
                    int slot = alloc.getSpillSlot(vid);
                    Instruction st;
                    st.opcode    = IrOpcode::SpillStore;
                    st.type      = fn_.vreg(vid).type;
                    st.spillSlot = slot;
                    st.uses.push_back(Operand::reg(vid));
                    newInsns.push_back(std::move(st));
                }
            }
        }

        blkPtr->insns = std::move(newInsns);
    }
}

} // namespace regalloc
} // namespace ptxra
