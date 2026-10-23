#pragma once

#include "core/graph.hpp"
#include "io/circuit_ir.hpp"

namespace zx {

class CircuitExtractor {
public:
    static CircuitIR extract(Graph g);
};

} // namespace zx
