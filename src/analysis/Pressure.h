#pragma once
#include "LiveInterval.h"
#include "../cfg/LoopInfo.h"
#include <vector>

namespace ptxra {
namespace analysis {

/// Register pressure sampled at a single program point.
struct PressurePoint {
    uint32_t programPoint  = 0;
    int      predPressure    = 0; ///< Live pred-class vregs at this point
    int      int32Pressure   = 0; ///< Live Int32-class vregs
    int      int64Pressure   = 0; ///< Live Int64-class vregs
    int      float32Pressure = 0; ///< Live Float32-class vregs
    int      float64Pressure = 0; ///< Live Float64-class vregs

    /// Total live vregs across all register classes.
    int total() const noexcept {
        return predPressure + int32Pressure + int64Pressure
             + float32Pressure + float64Pressure;
    }
};

/// Register pressure analysis result for a whole function.
struct PressureResult {
    std::vector<PressurePoint> points; ///< One entry per program point [0, numPoints)
    PressurePoint peak; ///< Program point with the highest total() pressure

    // Per-class peak values (max across all program points).
    int peakPred    = 0;
    int peakInt32   = 0;
    int peakInt64   = 0;
    int peakFloat32 = 0;
    int peakFloat64 = 0;

    /// Compute register pressure from live intervals.
    ///
    /// For each program point p, count the number of live intervals whose
    /// [start, end] range contains p, grouped by register class.
    static PressureResult compute(const ir::Function& fn,
                                   const LiveIntervals& intervals);
};

} // namespace analysis
} // namespace ptxra
