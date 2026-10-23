#pragma once

#include "io/circuit_ir.hpp"

namespace zx {

class PostOptimizer {
public:
    static CircuitIR optimize(const CircuitIR& ir);
};

} // namespace zx
