#pragma once
#include <string>
#include <vector>
#include <ostream>

namespace ptxra {
namespace report {

struct KernelComparisonRow {
    std::string kernelName;
    int budget = 255;
    // Linear scan
    int lsRegs = 0;
    int lsPreds = 0;
    int lsSpillStores = 0;
    int lsSpillLoads = 0;
    double lsSpillCost = 0.0;
    // Graph color
    int gcRegs = 0;
    int gcPreds = 0;
    int gcSpillStores = 0;
    int gcSpillLoads = 0;
    double gcSpillCost = 0.0;
    // Real ptxas -v (optional)
    bool hasPtxas = false;
    int ptxasRegs = 0;
    int ptxasSpillStores = 0;
    int ptxasSpillLoads = 0;
    int ptxasStackFrame = 0;
};

class ComparisonReporter {
public:
    static void emitCSV(const std::vector<KernelComparisonRow>& rows, std::ostream& os);
    static void emitMarkdown(const std::vector<KernelComparisonRow>& rows, std::ostream& os);
};

} // namespace report
} // namespace ptxra
