#pragma once
#include "../ir/Function.h"
#include "../analysis/Liveness.h"
#include "../analysis/Pressure.h"
#include "../regalloc/AllocationMap.h"
#include <string>
#include <ostream>

namespace ptxra {
namespace report {

class JSONReport {
public:
    static void emitFunction(const ir::Function& fn,
                             const analysis::LivenessResult* live,
                             const analysis::PressureResult* press,
                             const regalloc::AllocStats* stats,
                             const regalloc::AllocationMap* alloc,
                             std::ostream& os);
};

} // namespace report
} // namespace ptxra
