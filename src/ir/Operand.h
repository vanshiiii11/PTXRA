#pragma once
#include "VirtualReg.h"
#include <string>
#include <variant>
#include <optional>
#include <cstdint>

namespace ptxra {
namespace ir {

/// An immediate value (integer or float, stored as bit pattern / string)
struct ImmValue {
    std::string raw; // original spelling from PTX
    int64_t     ival = 0; // parsed integer value if applicable
    bool        isFloat = false;
};

/// A label reference (branch target)
struct LabelRef {
    std::string name;
};

/// An address expression: [baseReg + offset] or [globalSym + offset]
struct AddrExpr {
    VRegId      baseReg = InvalidVReg; // InvalidVReg if global symbol
    std::string globalSym;             // non-empty if global
    int64_t     offset  = 0;
};

/// Operand in the IR — can be a virtual register, immediate, label, or address.
using OperandData = std::variant<VRegId, ImmValue, LabelRef, AddrExpr>;

struct Operand {
    OperandData data;

    static Operand reg(VRegId id)              { return {id}; }
    static Operand imm(std::string r, int64_t v=0, bool f=false) {
        return {ImmValue{std::move(r), v, f}};
    }
    static Operand label(std::string n)        { return {LabelRef{std::move(n)}}; }
    static Operand addr(VRegId base, int64_t off=0) { return {AddrExpr{base, "", off}}; }
    static Operand addrGlobal(std::string sym, int64_t off=0) {
        return {AddrExpr{InvalidVReg, std::move(sym), off}};
    }

    bool isReg()   const { return std::holds_alternative<VRegId>(data); }
    bool isImm()   const { return std::holds_alternative<ImmValue>(data); }
    bool isLabel() const { return std::holds_alternative<LabelRef>(data); }
    bool isAddr()  const { return std::holds_alternative<AddrExpr>(data); }

    VRegId          asReg()   const { return std::get<VRegId>(data); }
    const ImmValue& asImm()   const { return std::get<ImmValue>(data); }
    const LabelRef& asLabel() const { return std::get<LabelRef>(data); }
    const AddrExpr& asAddr()  const { return std::get<AddrExpr>(data); }

    /// If this is a register operand, return its ID; else InvalidVReg.
    VRegId tryReg() const {
        if (isReg()) return asReg();
        return InvalidVReg;
    }

    std::string toString() const;
};

} // namespace ir
} // namespace ptxra
