#include "Instruction.h"
#include <sstream>

namespace ptxra {
namespace ir {

std::string iropcName(IrOpcode op) {
    switch (op) {
    case IrOpcode::Mov:        return "mov";
    case IrOpcode::Add:        return "add";
    case IrOpcode::Sub:        return "sub";
    case IrOpcode::Mul:        return "mul";
    case IrOpcode::Mad:        return "mad";
    case IrOpcode::Fma:        return "fma";
    case IrOpcode::Div:        return "div";
    case IrOpcode::Rem:        return "rem";
    case IrOpcode::Neg:        return "neg";
    case IrOpcode::Abs:        return "abs";
    case IrOpcode::Min:        return "min";
    case IrOpcode::Max:        return "max";
    case IrOpcode::And:        return "and";
    case IrOpcode::Or:         return "or";
    case IrOpcode::Xor:        return "xor";
    case IrOpcode::Not:        return "not";
    case IrOpcode::Shl:        return "shl";
    case IrOpcode::Shr:        return "shr";
    case IrOpcode::Cvt:        return "cvt";
    case IrOpcode::Cvta:       return "cvta";
    case IrOpcode::Setp:       return "setp";
    case IrOpcode::Selp:       return "selp";
    case IrOpcode::Ld:         return "ld";
    case IrOpcode::St:         return "st";
    case IrOpcode::Bra:        return "bra";
    case IrOpcode::CbrTrue:    return "cbr.t";
    case IrOpcode::CbrFalse:   return "cbr.f";
    case IrOpcode::Ret:        return "ret";
    case IrOpcode::Exit:       return "exit";
    case IrOpcode::BarSync:    return "bar.sync";
    case IrOpcode::Atom:       return "atom";
    case IrOpcode::SpillStore: return "spill.st";
    case IrOpcode::SpillLoad:  return "spill.ld";
    case IrOpcode::Unknown:    return "???";
    }
    return "???";
}

std::vector<VRegId> Instruction::defRegs() const {
    std::vector<VRegId> result;
    for (const auto& op : defs) {
        if (op.isReg()) result.push_back(op.asReg());
    }
    return result;
}

std::vector<VRegId> Instruction::useRegs() const {
    std::vector<VRegId> result;
    // Predicate guard counts as a use
    if (predReg) result.push_back(*predReg);
    for (const auto& op : uses) {
        if (op.isReg()) result.push_back(op.asReg());
        else if (op.isAddr()) {
            const auto& a = op.asAddr();
            if (a.baseReg != InvalidVReg) result.push_back(a.baseReg);
        }
    }
    return result;
}

std::string Instruction::toString() const {
    std::ostringstream os;
    if (predReg) {
        os << "@" << (predNeg ? "!" : "") << "v" << *predReg << " ";
    }
    os << iropcName(opcode);
    if (!defs.empty()) {
        os << " ";
        for (std::size_t i = 0; i < defs.size(); ++i) {
            if (i) os << "|";
            os << defs[i].toString();
        }
        if (!uses.empty()) os << ",";
    }
    for (std::size_t i = 0; i < uses.size(); ++i) {
        os << (i == 0 ? " " : ", ");
        os << uses[i].toString();
    }
    return os.str();
}

} // namespace ir
} // namespace ptxra
