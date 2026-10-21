#pragma once
#include <cstdint>
#include <string>

namespace ptxra {
namespace ir {

/// Register class — determines which physical register pool a virtual register belongs to.
enum class RegClass {
    Pred,   // 1-bit predicate registers (%p)
    Int32,  // 32-bit integer (%r)
    Int64,  // 64-bit integer (%rd)
    Float32,// 32-bit float (%f)
    Float64,// 64-bit float (%fd)
};

std::string regClassName(RegClass rc);

/// A type as understood by the IR (after widening narrow types).
enum class IrType {
    Pred,
    Int32,
    Int64,
    Float32,
    Float64,
    Unknown,
};

std::string irTypeName(IrType t);
RegClass    irTypeToRegClass(IrType t);
IrType      irTypeFromBits(int bits, bool isFp, bool isSigned);

/// A virtual register.  Once the allocator runs, it gains a physical assignment.
using VRegId = uint32_t;
constexpr VRegId InvalidVReg = ~0u;

struct VirtualReg {
    VRegId   id       = InvalidVReg;
    IrType   type     = IrType::Unknown;
    RegClass regClass = RegClass::Int32;
    std::string name; // original PTX name for diagnostics (e.g., "%r5")

    RegClass getRegClass() const { return regClass; }
};

} // namespace ir
} // namespace ptxra
