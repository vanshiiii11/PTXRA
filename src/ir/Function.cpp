#include "Function.h"
#include <unordered_set>
#include <stack>

namespace ptxra {
namespace ir {

/// Compute RPO (reverse post-order) via iterative DFS.
std::vector<BasicBlock*> Function::rpo() const {
    if (blocks.empty()) return {};

    std::vector<BasicBlock*> postOrder;
    std::unordered_set<BlockId> visited;
    // Iterative DFS using explicit stack to avoid recursion limits
    std::stack<std::pair<BasicBlock*, std::size_t>> stack; // {block, succIdx}
    auto* startBlock = blocks[0].get();
    stack.push({startBlock, 0});
    visited.insert(startBlock->id);

    while (!stack.empty()) {
        auto& [blk, idx] = stack.top();
        if (idx < blk->succs.size()) {
            BlockId succId = blk->succs[idx++];
            if (visited.find(succId) == visited.end()) {
                visited.insert(succId);
                if (auto* s = const_cast<Function*>(this)->blockById(succId)) {
                    stack.push({s, 0});
                }
            }
        } else {
            postOrder.push_back(blk);
            stack.pop();
        }
    }

    // Unreachable blocks: add them at the end in original order
    for (const auto& b : blocks) {
        if (visited.find(b->id) == visited.end()) {
            postOrder.push_back(b.get());
        }
    }

    // Reverse post-order
    std::vector<BasicBlock*> result(postOrder.rbegin(), postOrder.rend());
    return result;
}

} // namespace ir
} // namespace ptxra
