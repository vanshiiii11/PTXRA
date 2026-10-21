#pragma once
#include "../ir/Function.h"
#include <ostream>
#include <string>

// Forward-declare LivenessResult so DOTEmitter can accept a pointer to it
// without requiring the full Liveness.h header.  When Liveness.h exists,
// callers pass a real pointer; until then, nullptr is passed and annotations
// are simply omitted.
namespace ptxra {
namespace analysis {
struct LivenessResult;
} // namespace analysis
} // namespace ptxra

namespace ptxra {
namespace cfg {

/// Emits Graphviz DOT format for a function's CFG.
/// Optionally annotates blocks with live-in/live-out sets.
class DOTEmitter {
public:
    void emit(const ir::Function& fn, std::ostream& out,
              const analysis::LivenessResult* liveness = nullptr);

private:
    std::string escape(const std::string& s);
    std::string blockLabel(const ir::BasicBlock& blk, const ir::Function& fn,
                            const analysis::LivenessResult* liveness);
};

} // namespace cfg
} // namespace ptxra
