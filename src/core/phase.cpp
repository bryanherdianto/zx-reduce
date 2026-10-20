#include "core/phase.hpp"
#include <sstream>

namespace zx {

std::string Phase::to_string() const {
    if (is_zero()) {
        return "0";
    }
    if (num == 1 && den == 1) {
        return "pi";
    }
    if (num == 1) {
        return "pi/" + std::to_string(den);
    }
    if (den == 1) {
        return std::to_string(num) + "*pi";
    }
    return std::to_string(num) + "*pi/" + std::to_string(den);
}

} // namespace zx
