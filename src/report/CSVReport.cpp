#include "CSVReport.h"
#include <iomanip>

namespace ptxra {
namespace report {

void ComparisonReporter::emitCSV(const std::vector<KernelComparisonRow>& rows, std::ostream& os) {
    os << "Kernel,Budget,LS_Regs,LS_Preds,LS_SpillStores,LS_SpillLoads,LS_SpillCost,GC_Regs,GC_Preds,GC_SpillStores,GC_SpillLoads,GC_SpillCost,"
       << "PTXAS_Regs,PTXAS_SpillStores,PTXAS_SpillLoads,PTXAS_StackBytes\n";
    for (const auto& r : rows) {
        os << r.kernelName << ","
           << r.budget << ","
           << r.lsRegs << "," << r.lsPreds << "," << r.lsSpillStores << "," << r.lsSpillLoads << "," << static_cast<long long>(r.lsSpillCost) << ","
           << r.gcRegs << "," << r.gcPreds << "," << r.gcSpillStores << "," << r.gcSpillLoads << "," << static_cast<long long>(r.gcSpillCost) << ",";
        if (r.hasPtxas) {
            os << r.ptxasRegs << "," << r.ptxasSpillStores << ","
               << r.ptxasSpillLoads << "," << r.ptxasStackFrame;
        } else {
            os << "N/A,N/A,N/A,N/A";
        }
        os << "\n";
    }
}

void ComparisonReporter::emitMarkdown(const std::vector<KernelComparisonRow>& rows, std::ostream& os) {
    os << "| Kernel | Budget | LS Regs (Preds) | LS Spills (st/ld) | LS Spill Cost | GC Regs (Preds) | GC Spills (st/ld) | GC Spill Cost | ptxas Regs | ptxas Spills (st/ld) | ptxas Stack |\n";
    os << "| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n";
    for (const auto& r : rows) {
        os << "| " << r.kernelName << " | "
           << r.budget << " | "
           << r.lsRegs << " (" << r.lsPreds << "p) | "
           << r.lsSpillStores << "/" << r.lsSpillLoads << " | "
           << static_cast<long long>(r.lsSpillCost) << " | "
           << r.gcRegs << " (" << r.gcPreds << "p) | "
           << r.gcSpillStores << "/" << r.gcSpillLoads << " | "
           << static_cast<long long>(r.gcSpillCost) << " | ";
        if (r.hasPtxas) {
            os << r.ptxasRegs << " | " << r.ptxasSpillStores << "/" << r.ptxasSpillLoads
               << " | " << r.ptxasStackFrame << "B |";
        } else {
            os << "N/A | N/A | N/A |";
        }
        os << "\n";
    }
}

} // namespace report
} // namespace ptxra
