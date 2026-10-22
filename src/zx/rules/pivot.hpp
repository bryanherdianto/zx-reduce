#pragma once

#include "core/graph.hpp"
#include <vector>
#include <unordered_set>

namespace zx {

struct PivotMatch {
    VertexId u;
    VertexId v;
};

class PivotRule {
public:
    static std::vector<PivotMatch> find(const Graph& g) {
        std::vector<PivotMatch> matches;
        for (size_t i = 0; i < g.vertices.size(); ++i) {
            if (!g.vertices[i].active) continue;
            VertexId u = static_cast<VertexId>(i);
            if (g.vertices[u].type != VertexType::Z) continue;
            if (!g.vertices[u].phase.is_zero() && !g.vertices[u].phase.is_pauli()) continue;

            // Check if u has boundary neighbors
            bool u_has_boundary = false;
            for (VertexId nbr : g.neighbors(u)) {
                if (g.vertices[nbr].type == VertexType::Boundary) {
                    u_has_boundary = true;
                    break;
                }
            }
            if (u_has_boundary) continue;

            for (VertexId v : g.neighbors(u)) {
                if (u >= v) continue;
                if (!g.is_valid_vertex(v) || g.vertices[v].type != VertexType::Z) continue;
                if (!g.vertices[v].phase.is_zero() && !g.vertices[v].phase.is_pauli()) continue;

                // Check edge is Hadamard
                auto ec = g.edge_count(u, v);
                if (ec.n_hadamard == 0) continue;

                bool v_has_boundary = false;
                for (VertexId nbr : g.neighbors(v)) {
                    if (g.vertices[nbr].type == VertexType::Boundary) {
                        v_has_boundary = true;
                        break;
                    }
                }
                if (v_has_boundary) continue;

                matches.push_back({u, v});
            }
        }
        return matches;
    }

    static bool apply(Graph& g, const PivotMatch& m) {
        VertexId u = m.u;
        VertexId v = m.v;

        if (!g.is_valid_vertex(u) || !g.is_valid_vertex(v)) return false;
        if (g.vertices[u].type != VertexType::Z || g.vertices[v].type != VertexType::Z) return false;
        if (!g.vertices[u].phase.is_zero() && !g.vertices[u].phase.is_pauli()) return false;
        if (!g.vertices[v].phase.is_zero() && !g.vertices[v].phase.is_pauli()) return false;

        auto u_nbrs_vec = g.neighbors(u);
        auto v_nbrs_vec = g.neighbors(v);

        std::unordered_set<VertexId> u_nbrs(u_nbrs_vec.begin(), u_nbrs_vec.end());
        std::unordered_set<VertexId> v_nbrs(v_nbrs_vec.begin(), v_nbrs_vec.end());

        // Check boundary neighbors
        for (VertexId nbr : u_nbrs) {
            if (g.vertices[nbr].type == VertexType::Boundary) return false;
        }
        for (VertexId nbr : v_nbrs) {
            if (g.vertices[nbr].type == VertexType::Boundary) return false;
        }

        std::vector<VertexId> A;
        std::vector<VertexId> B;
        std::vector<VertexId> C;

        for (VertexId nbr : u_nbrs) {
            if (nbr == v) continue;
            if (v_nbrs.count(nbr)) {
                C.push_back(nbr);
            } else {
                A.push_back(nbr);
            }
        }

        for (VertexId nbr : v_nbrs) {
            if (nbr == u) continue;
            if (!u_nbrs.count(nbr)) {
                B.push_back(nbr);
            }
        }

        auto toggle_hadamard = [&](VertexId x, VertexId y) {
            if (x == y) return;
            auto ec = g.edge_count(x, y);
            if (ec.n_hadamard > 0) {
                g.remove_edge(x, y, EdgeType::Hadamard);
            } else {
                g.add_edge(x, y, EdgeType::Hadamard);
            }
        };

        // Toggle A x B
        for (VertexId a : A) {
            for (VertexId b : B) {
                toggle_hadamard(a, b);
            }
        }

        // Toggle B x C
        for (VertexId b : B) {
            for (VertexId c : C) {
                toggle_hadamard(b, c);
            }
        }

        // Toggle A x C
        for (VertexId a : A) {
            for (VertexId c : C) {
                toggle_hadamard(a, c);
            }
        }

        // Phase updates according to PyZX / Duncan et al.
        Phase u_ph = g.vertices[u].phase;
        Phase v_ph = g.vertices[v].phase;

        for (VertexId b : B) {
            g.vertices[b].phase += u_ph;
        }
        for (VertexId a : A) {
            g.vertices[a].phase += v_ph;
        }
        for (VertexId c : C) {
            g.vertices[c].phase += Phase::pi() + u_ph + v_ph;
        }

        g.scalar.div_two(1);
        g.remove_vertex(u);
        g.remove_vertex(v);
        return true;
    }
};

} // namespace zx
