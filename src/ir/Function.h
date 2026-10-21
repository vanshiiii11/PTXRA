#pragma once
#include "BasicBlock.h"
#include "VirtualReg.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <optional>

namespace ptxra {
namespace ir {

/// Represents a single PTX kernel (or device function) in IR form.
struct Function {
    std::string name;
    bool        isEntry = true;

    // All basic blocks. blocks[0] is the entry block.
    std::vector<std::unique_ptr<BasicBlock>> blocks;

    // Virtual register table: id → VirtualReg info
    std::vector<VirtualReg> vregs; // indexed by VRegId

    // Local spill slots (allocated as needed by allocator)
    int spillSlotCount = 0;

    // ---- Block accessors ----
    BasicBlock*       entry()       { return blocks.empty() ? nullptr : blocks[0].get(); }
    const BasicBlock* entry() const { return blocks.empty() ? nullptr : blocks[0].get(); }

    BasicBlock* blockById(BlockId id) {
        for (auto& b : blocks) if (b->id == id) return b.get();
        return nullptr;
    }
    const BasicBlock* blockById(BlockId id) const {
        for (const auto& b : blocks) if (b->id == id) return b.get();
        return nullptr;
    }

    /// Return blocks in RPO order (entry first, then DFS post-order reversed).
    std::vector<BasicBlock*> rpo() const;

    // ---- VReg accessors ----
    const VirtualReg& vreg(VRegId id) const { return vregs[id]; }
    VirtualReg&       vreg(VRegId id)       { return vregs[id]; }

    VRegId newVReg(IrType ty, std::string name = "") {
        VRegId id = static_cast<VRegId>(vregs.size());
        VirtualReg vr;
        vr.id       = id;
        vr.type     = ty;
        vr.regClass = irTypeToRegClass(ty);
        vr.name     = std::move(name);
        vregs.push_back(vr);
        return id;
    }

    std::size_t numVRegs() const { return vregs.size(); }
};

} // namespace ir
} // namespace ptxra
