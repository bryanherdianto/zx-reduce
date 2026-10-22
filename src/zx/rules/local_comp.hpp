#pragma once

#include "core/graph.hpp"
#include <vector>

namespace zx {

struct LocalCompMatch {
    VertexId v;
};

class LocalCompRule {
public:
    static std::vector<LocalCompMatch> find(const Graph& g) {
        std::vector<LocalCompMatch> matches;
        for (size_t i = 0; i < g.vertices.size(); ++i) {
            if (!g.vertices[i].active) continue;
            VertexId v = static_cast<VertexId>(i);
            if (g.vertices[v].type != VertexType::Z) continue;
            if (!g.vertices[v].phase.is_proper_clifford()) continue;

            // Check that all neighbors are interior Z-spiders (no boundary neighbors directly)
            bool valid = true;
            for (VertexId nbr : g.neighbors(v)) {
                if (g.vertices[nbr].type != VertexType::Z) {
                    valid = false;
                    break;
                }
            }
            if (valid) {
                matches.push_back({v});
            }
        }
        return matches;
    }

    static bool apply(Graph& g, const LocalCompMatch& m) {
        VertexId v = m.v;
        if (!g.is_valid_vertex(v) || g.vertices[v].type != VertexType::Z || !g.vertices[v].phase.is_proper_clifford()) {
            return false;
        }

        auto nbrs = g.neighbors(v);
        for (VertexId nbr : nbrs) {
            if (g.vertices[nbr].type != VertexType::Z) return false;
        }

        Phase v_phase = g.vertices[v].phase;
        Phase neg_v_phase = -v_phase;

        // Invert connectivity among neighbors of v (toggle Hadamard edges)
        for (size_t i = 0; i < nbrs.size(); ++i) {
            for (size_t j = i + 1; j < nbrs.size(); ++j) {
                VertexId u1 = nbrs[i];
                VertexId u2 = nbrs[j];
                auto ec = g.edge_count(u1, u2);
                if (ec.n_hadamard > 0) {
                    g.remove_edge(u1, u2, EdgeType::Hadamard);
                } else {
                    g.add_edge(u1, u2, EdgeType::Hadamard);
                }
            }
        }

        // Adjust phases of neighbors
        for (VertexId nbr : nbrs) {
            g.vertices[nbr].phase += neg_v_phase;
        }

        // Update scalar
        if (v_phase == Phase::half_pi()) {
            g.scalar.mul_phase(Phase(1, 4));
        } else {
            g.scalar.mul_phase(Phase(7, 4));
        }
        g.remove_vertex(v);
        return true;
    }
};

} // namespace zx
