#include "zx/simplify.hpp"
#include "zx/to_graph_like.hpp"
#include "zx/rules/local_comp.hpp"
#include "zx/rules/pivot.hpp"
#include "zx/rules/gadget.hpp"

namespace zx {

void Simplifier::clifford_reduce(Graph& g) {
    ToGraphLike::convert(g);

    bool progress = true;
    size_t iters = 0;
    while (progress && iters < 1000) {
        progress = false;
        iters++;

        // 1. Local complementation
        auto lc_matches = LocalCompRule::find(g);
        for (const auto& m : lc_matches) {
            if (LocalCompRule::apply(g, m)) {
                progress = true;
                ToGraphLike::convert(g);
                break;
            }
        }
        if (progress) continue;

        // 2. Pivoting
        auto p_matches = PivotRule::find(g);
        for (const auto& m : p_matches) {
            if (PivotRule::apply(g, m)) {
                progress = true;
                ToGraphLike::convert(g);
                break;
            }
        }
    }
}

void Simplifier::full_reduce(Graph& g) {
    clifford_reduce(g);

    bool progress = true;
    size_t iters = 0;
    while (progress && iters < 1000) {
        progress = false;
        iters++;

        // Gadget fusions
        auto gf_matches = GadgetRule::find_fuse(g);
        for (const auto& m : gf_matches) {
            if (GadgetRule::apply_fuse(g, m)) {
                progress = true;
                break;
            }
        }

        if (progress) {
            clifford_reduce(g);
        }
    }
}

} // namespace zx
