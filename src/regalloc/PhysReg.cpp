#include "PhysReg.h"

namespace ptxra {
namespace regalloc {

std::string PhysReg::toString() const {
    if (!isValid()) return "%invalid";
    if (isPred()) return "%p" + std::to_string(slot);
    if (width == 2) {
        return "%r" + std::to_string(slot) + ":%r" + std::to_string(slot + 1);
    }
    return "%r" + std::to_string(slot);
}

} // namespace regalloc
} // namespace ptxra
