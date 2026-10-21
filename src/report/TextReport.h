#pragma once
#include "../ir/Function.h"
#include "../analysis/Liveness.h"
#include "../analysis/Pressure.h"
#include "../regalloc/AllocationMap.h"
#include <ostream>

namespace ptxra {
namespace report {

class TextReport {
public:
    static void printFunctionCFG(const ir::Function& fn, std::ostream& os);
    static void printLiveness(const ir::Function& fn, const analysis::LivenessResult& live, std::ostream& os);
    static void printPressure(const ir::Function& fn, const analysis::PressureResult& press, std::ostream& os);
    static void printAllocation(const ir::Function& fn, const regalloc::AllocStats& stats,
                                const regalloc::AllocationMap& alloc, std::ostream& os);
};

} // namespace report
} // namespace ptxra
