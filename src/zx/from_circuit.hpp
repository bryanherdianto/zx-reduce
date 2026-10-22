#pragma once

#include "core/graph.hpp"
#include "io/circuit_ir.hpp"

namespace zx {

class CircuitToGraph {
public:
    static Graph convert(const CircuitIR& ir);
};

} // namespace zx
