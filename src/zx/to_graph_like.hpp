#pragma once

#include "core/graph.hpp"

namespace zx {

class ToGraphLike {
public:
    static void convert(Graph& g);

private:
    static bool fuse_spiders(Graph& g);
    static bool remove_identities(Graph& g);
    static void color_change(Graph& g);
    static void convert_interior_simple_to_hadamard(Graph& g);
};

} // namespace zx
