#include "zx/to_graph_like.hpp"
#include <vector>

namespace zx {

void ToGraphLike::convert(Graph& g) {
    // 1. Convert all X spiders to Z spiders via color change
    color_change(g);

    // 2. Loop fusion and identity removal until convergence
    bool changed = true;
    while (changed) {
        changed = false;
        if (fuse_spiders(g)) changed = true;
        if (remove_identities(g)) changed = true;
    }

    // 3. Ensure any remaining simple edges between interior Z spiders become Hadamard
    convert_interior_simple_to_hadamard(g);

    // 4. Final fusion & identity cleanup
    changed = true;
    while (changed) {
        changed = false;
        if (fuse_spiders(g)) changed = true;
        if (remove_identities(g)) changed = true;
    }
}

void ToGraphLike::color_change(Graph& g) {
    for (size_t i = 0; i < g.vertices.size(); ++i) {
        if (!g.vertices[i].active) continue;
        if (g.vertices[i].type == VertexType::X) {
            VertexId v = static_cast<VertexId>(i);
            g.vertices[v].type = VertexType::Z;

            // Toggle edge types on all incident edges
            // Collect neighbors first to avoid mutating while iterating
            auto nbrs = g.vertices[v].neighbors;
            for (const auto& [nbr, count] : nbrs) {
                EdgeCount new_count{count.n_hadamard, count.n_simple};
                g.set_edge_count(v, nbr, new_count);
            }
        }
    }
}

bool ToGraphLike::fuse_spiders(Graph& g) {
    bool changed = false;

    for (size_t i = 0; i < g.vertices.size(); ++i) {
        if (!g.vertices[i].active) continue;
        VertexId u = static_cast<VertexId>(i);
        if (g.vertices[u].type == VertexType::Boundary) continue;

        auto nbrs = g.neighbors(u);
        for (VertexId v : nbrs) {
            if (!g.is_valid_vertex(u) || !g.is_valid_vertex(v)) continue;
            if (u >= v) continue;
            if (g.vertices[v].type == VertexType::Boundary) continue;

            // Both u and v are interior Z-spiders
            auto ec = g.edge_count(u, v);
            if (ec.n_simple > 0) {
                // Merge v into u
                g.vertices[u].phase += g.vertices[v].phase;

                // Transfer neighbors of v to u
                auto v_nbrs = g.vertices[v].neighbors;
                for (const auto& [nbr, count] : v_nbrs) {
                    if (nbr == u) continue;
                    auto cur_u_nbr = g.edge_count(u, nbr);
                    EdgeCount merged{
                        static_cast<uint8_t>(cur_u_nbr.n_simple + count.n_simple),
                        static_cast<uint8_t>(cur_u_nbr.n_hadamard + count.n_hadamard)
                    };
                    g.set_edge_count(u, nbr, merged);
                    g.normalize_edges(u, nbr);
                }

                g.remove_vertex(v);
                changed = true;
                break;
            }
        }
    }

    return changed;
}

bool ToGraphLike::remove_identities(Graph& g) {
    bool changed = false;

    for (size_t i = 0; i < g.vertices.size(); ++i) {
        if (!g.vertices[i].active) continue;
        VertexId v = static_cast<VertexId>(i);
        if (g.vertices[v].type != VertexType::Z) continue;
        if (!g.vertices[v].phase.is_zero()) continue;

        // Check if degree is 2 and no self loops
        if (g.degree(v) != 2 || g.vertices[v].self_simple > 0 || g.vertices[v].self_hadamard > 0) {
            continue;
        }

        auto nbrs = g.neighbors(v);
        if (nbrs.size() != 2) continue; // Not degree 2 with 2 distinct neighbors

        VertexId u1 = nbrs[0];
        VertexId u2 = nbrs[1];

        // Do not remove identity if both neighbors are boundaries (keep wire connected)
        if (g.vertices[u1].type == VertexType::Boundary && g.vertices[u2].type == VertexType::Boundary) {
            continue;
        }

        auto ec1 = g.edge_count(u1, v);
        auto ec2 = g.edge_count(v, u2);

        // Determine edge type between u1 and u2:
        // simple + simple -> simple
        // simple + hadamard -> hadamard
        // hadamard + hadamard -> simple
        bool is_hadamard1 = (ec1.n_hadamard > 0);
        bool is_hadamard2 = (ec2.n_hadamard > 0);
        if (is_hadamard1 && is_hadamard2) {
            // In graph-like diagrams, H-Z(0)-H is an interior wire; removing it would create an interior simple edge
            continue;
        }
        EdgeType new_type = (is_hadamard1 ^ is_hadamard2) ? EdgeType::Hadamard : EdgeType::Simple;

        g.remove_vertex(v);
        g.add_edge(u1, u2, new_type);
        g.normalize_edges(u1, u2);
        changed = true;
    }

    return changed;
}

void ToGraphLike::convert_interior_simple_to_hadamard(Graph& g) {
    for (size_t i = 0; i < g.vertices.size(); ++i) {
        if (!g.vertices[i].active) continue;
        VertexId u = static_cast<VertexId>(i);
        if (g.vertices[u].type == VertexType::Boundary) continue;

        auto nbrs = g.neighbors(u);
        for (VertexId v : nbrs) {
            if (!g.is_valid_vertex(u) || !g.is_valid_vertex(v)) continue;
            if (u >= v) continue;
            if (g.vertices[v].type == VertexType::Boundary) continue;

            auto ec = g.edge_count(u, v);
            if (ec.n_simple > 0) {
                // Insert a dummy Z(0) vertex with Hadamard edges:
                // u --(simple)-- v  ==>  u --(Hadamard)-- w(0) --(Hadamard)-- v
                // But wait! If u and v are both Z, u --(simple)-- v can be fused directly!
                // If it wasn't fused, fuse it now:
                g.vertices[u].phase += g.vertices[v].phase;
                auto v_nbrs = g.vertices[v].neighbors;
                for (const auto& [nbr, count] : v_nbrs) {
                    if (nbr == u) continue;
                    auto cur_u_nbr = g.edge_count(u, nbr);
                    EdgeCount merged{
                        static_cast<uint8_t>(cur_u_nbr.n_simple + count.n_simple),
                        static_cast<uint8_t>(cur_u_nbr.n_hadamard + count.n_hadamard)
                    };
                    g.set_edge_count(u, nbr, merged);
                    g.normalize_edges(u, nbr);
                }
                g.remove_vertex(v);
                break;
            }
        }
    }
}

} // namespace zx
