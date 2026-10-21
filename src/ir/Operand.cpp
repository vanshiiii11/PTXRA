#include "Operand.h"
#include <sstream>

namespace ptxra {
namespace ir {

std::string Operand::toString() const {
    if (isReg()) {
        return "v" + std::to_string(asReg());
    }
    if (isImm()) {
        return asImm().raw;
    }
    if (isLabel()) {
        return asLabel().name;
    }
    if (isAddr()) {
        const auto& a = asAddr();
        std::string s = "[";
        if (a.baseReg != InvalidVReg) s += "v" + std::to_string(a.baseReg);
        else                           s += a.globalSym;
        if (a.offset > 0)  s += "+" + std::to_string(a.offset);
        if (a.offset < 0)  s += std::to_string(a.offset);
        s += "]";
        return s;
    }
    return "??";
}

} // namespace ir
} // namespace ptxra
