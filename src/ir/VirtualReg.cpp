#include "VirtualReg.h"
#include "../ast/AST.h"

namespace ptxra {
namespace ir {

std::string regClassName(RegClass rc) {
    switch (rc) {
    case RegClass::Pred:    return "pred";
    case RegClass::Int32:   return "int32";
    case RegClass::Int64:   return "int64";
    case RegClass::Float32: return "float32";
    case RegClass::Float64: return "float64";
    }
    return "??";
}

std::string irTypeName(IrType t) {
    switch (t) {
    case IrType::Pred:    return "pred";
    case IrType::Int32:   return "i32";
    case IrType::Int64:   return "i64";
    case IrType::Float32: return "f32";
    case IrType::Float64: return "f64";
    case IrType::Unknown: return "??";
    }
    return "??";
}

RegClass irTypeToRegClass(IrType t) {
    switch (t) {
    case IrType::Pred:    return RegClass::Pred;
    case IrType::Int32:   return RegClass::Int32;
    case IrType::Int64:   return RegClass::Int64;
    case IrType::Float32: return RegClass::Float32;
    case IrType::Float64: return RegClass::Float64;
    case IrType::Unknown: return RegClass::Int32; // fallback
    }
    return RegClass::Int32;
}

IrType irTypeFromBits(int bits, bool isFp, bool /*isSigned*/) {
    if (bits == 1)  return IrType::Pred;
    if (isFp) {
        if (bits <= 32) return IrType::Float32;
        return IrType::Float64;
    }
    if (bits <= 32) return IrType::Int32;
    return IrType::Int64;
}

} // namespace ir
} // namespace ptxra
