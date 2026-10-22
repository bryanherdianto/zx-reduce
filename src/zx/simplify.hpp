#pragma once

#include "core/graph.hpp"

namespace zx {

class Simplifier {
public:
    static void full_reduce(Graph& g);
    static void clifford_reduce(Graph& g);
};

} // namespace zx
