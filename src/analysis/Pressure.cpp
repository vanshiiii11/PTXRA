#include "Pressure.h"
#include <algorithm>

namespace ptxra {
namespace analysis {

// ---------------------------------------------------------------------------
// PressureResult::compute
// ---------------------------------------------------------------------------

PressureResult PressureResult::compute(const ir::Function& fn,
                                        const LiveIntervals& intervals)
{
    const uint32_t numPoints = intervals.numPoints;

    PressureResult result;
    result.points.resize(numPoints); // one PressurePoint per program point

    // Initialise program-point indices.
    for (uint32_t p = 0; p < numPoints; ++p)
        result.points[p].programPoint = p;

    // -----------------------------------------------------------------------
    // For each live interval, add 1 to the appropriate register-class counter
    // for every program point it covers.
    //
    // A simple O(intervals × numPoints) sweep is acceptable for the typical
    // PTX kernel sizes encountered in practice (hundreds of vregs, thousands
    // of program points).  A sweep-line algorithm would be O(n log n) if
    // needed in the future.
    // -----------------------------------------------------------------------
    for (const LiveInterval& iv : intervals.intervals) {
        if (iv.start > iv.end) continue; // unused vreg, skip

        const ir::RegClass rc = fn.vreg(iv.vreg).regClass;

        const uint32_t lo = iv.start;
        const uint32_t hi = std::min(iv.end, numPoints > 0 ? numPoints - 1u : 0u);

        for (uint32_t p = lo; p <= hi; ++p) {
            PressurePoint& pt = result.points[p];
            switch (rc) {
                case ir::RegClass::Pred:    ++pt.predPressure;    break;
                case ir::RegClass::Int32:   ++pt.int32Pressure;   break;
                case ir::RegClass::Int64:   ++pt.int64Pressure;   break;
                case ir::RegClass::Float32: ++pt.float32Pressure; break;
                case ir::RegClass::Float64: ++pt.float64Pressure; break;
                // No default: all enum values handled; compiler warns on new ones.
            }
        }
    }

    // -----------------------------------------------------------------------
    // Find peak and per-class peaks.
    // -----------------------------------------------------------------------
    result.peak = PressurePoint{}; // zero-initialised; programPoint = 0
    for (const PressurePoint& pt : result.points) {
        // Update per-class peaks.
        result.peakPred    = std::max(result.peakPred,    pt.predPressure);
        result.peakInt32   = std::max(result.peakInt32,   pt.int32Pressure);
        result.peakInt64   = std::max(result.peakInt64,   pt.int64Pressure);
        result.peakFloat32 = std::max(result.peakFloat32, pt.float32Pressure);
        result.peakFloat64 = std::max(result.peakFloat64, pt.float64Pressure);

        // Update overall peak (highest total() wins).
        if (pt.total() > result.peak.total())
            result.peak = pt;
    }

    return result;
}

} // namespace analysis
} // namespace ptxra
