#include "core/graph.hpp"
#include <algorithm>

namespace zx {

VertexId Graph::add_vertex(VertexType type, Phase phase, int32_t qubit, int32_t row) {
    VertexId id;
    if (!free_list.empty()) {
        id = free_list.back();
        free_list.pop_back();
        vertices[id] = VertexData{type, phase, qubit, row, true, 0, 0, {}};
    } else {
        id = static_cast<VertexId>(vertices.size());
        vertices.push_back(VertexData{type, phase, qubit, row, true, 0, 0, {}});
    }
    return id;
}

void Graph::remove_vertex(VertexId v) {
    if (!is_valid_vertex(v)) return;

    // Disconnect from all neighbors
    for (auto& [nbr, count] : vertices[v].neighbors) {
        if (is_valid_vertex(nbr)) {
            vertices[nbr].neighbors.erase(v);
        }
    }

    vertices[v].neighbors.clear();
    vertices[v].active = false;
    free_list.push_back(v);

    // Remove from inputs/outputs if present
    inputs.erase(std::remove(inputs.begin(), inputs.end(), v), inputs.end());
    outputs.erase(std::remove(outputs.begin(), outputs.end(), v), outputs.end());
}

void Graph::add_edge(VertexId u, VertexId v, EdgeType type) {
    if (!is_valid_vertex(u) || !is_valid_vertex(v)) return;

    if (u == v) {
        if (type == EdgeType::Simple) {
            vertices[u].self_simple++;
        } else {
            vertices[u].self_hadamard++;
        }
        normalize_self_loops(u);
        return;
    }

    auto& ec_u = vertices[u].neighbors[v];
    auto& ec_v = vertices[v].neighbors[u];

    if (type == EdgeType::Simple) {
        ec_u.n_simple++;
        ec_v.n_simple++;
    } else {
        ec_u.n_hadamard++;
        ec_v.n_hadamard++;
    }

    normalize_edges(u, v);
}

void Graph::remove_edge(VertexId u, VertexId v, EdgeType type) {
    if (!is_valid_vertex(u) || !is_valid_vertex(v)) return;

    if (u == v) {
        if (type == EdgeType::Simple && vertices[u].self_simple > 0) {
            vertices[u].self_simple--;
        } else if (type == EdgeType::Hadamard && vertices[u].self_hadamard > 0) {
            vertices[u].self_hadamard--;
        }
        return;
    }

    auto it_u = vertices[u].neighbors.find(v);
    auto it_v = vertices[v].neighbors.find(u);

    if (it_u != vertices[u].neighbors.end()) {
        if (type == EdgeType::Simple && it_u->second.n_simple > 0) {
            it_u->second.n_simple--;
        } else if (type == EdgeType::Hadamard && it_u->second.n_hadamard > 0) {
            it_u->second.n_hadamard--;
        }
        if (it_u->second.empty()) {
            vertices[u].neighbors.erase(it_u);
        }
    }

    if (it_v != vertices[v].neighbors.end()) {
        if (type == EdgeType::Simple && it_v->second.n_simple > 0) {
            it_v->second.n_simple--;
        } else if (type == EdgeType::Hadamard && it_v->second.n_hadamard > 0) {
            it_v->second.n_hadamard--;
        }
        if (it_v->second.empty()) {
            vertices[v].neighbors.erase(it_v);
        }
    }
}

void Graph::set_edge_count(VertexId u, VertexId v, EdgeCount ec) {
    if (!is_valid_vertex(u) || !is_valid_vertex(v) || u == v) return;

    if (ec.empty()) {
        vertices[u].neighbors.erase(v);
        vertices[v].neighbors.erase(u);
    } else {
        vertices[u].neighbors[v] = ec;
        vertices[v].neighbors[u] = ec;
    }
}

EdgeCount Graph::edge_count(VertexId u, VertexId v) const {
    if (!is_valid_vertex(u) || !is_valid_vertex(v)) return EdgeCount{};
    if (u == v) {
        return EdgeCount{vertices[u].self_simple, vertices[u].self_hadamard};
    }
    auto it = vertices[u].neighbors.find(v);
    if (it == vertices[u].neighbors.end()) {
        return EdgeCount{};
    }
    return it->second;
}

bool Graph::has_edge(VertexId u, VertexId v) const {
    if (!is_valid_vertex(u) || !is_valid_vertex(v)) return false;
    if (u == v) {
        return vertices[u].self_simple > 0 || vertices[u].self_hadamard > 0;
    }
    auto it = vertices[u].neighbors.find(v);
    return it != vertices[u].neighbors.end() && !it->second.empty();
}

size_t Graph::degree(VertexId v) const {
    if (!is_valid_vertex(v)) return 0;
    size_t d = 0;
    for (const auto& [nbr, count] : vertices[v].neighbors) {
        d += count.n_simple + count.n_hadamard;
    }
    d += 2 * static_cast<size_t>(vertices[v].self_simple + vertices[v].self_hadamard);
    return d;
}

std::vector<VertexId> Graph::neighbors(VertexId v) const {
    std::vector<VertexId> res;
    if (!is_valid_vertex(v)) return res;
    res.reserve(vertices[v].neighbors.size());
    for (const auto& [nbr, count] : vertices[v].neighbors) {
        if (!count.empty()) {
            res.push_back(nbr);
        }
    }
    return res;
}

void Graph::normalize_edges(VertexId u, VertexId v) {
    if (!is_valid_vertex(u) || !is_valid_vertex(v) || u == v) return;

    auto it_u = vertices[u].neighbors.find(v);
    if (it_u == vertices[u].neighbors.end()) return;

    // Hopf law for same-color spiders:
    // If both u and v are Z-spiders (or both X-spiders):
    // Parallel simple edges: 2 parallel simple edges between Z spiders merge to 0 edges (with scalar adjustment)
    if (vertices[u].type == vertices[v].type && vertices[u].type != VertexType::Boundary) {
        // Parallel simple edges
        if (it_u->second.n_simple >= 2) {
            scalar.mul_two(it_u->second.n_simple / 2);
            it_u->second.n_simple %= 2;
        }
        // Parallel Hadamard edges: 2 Hadamard edges cancel to 0 (H * H = I)
        if (it_u->second.n_hadamard >= 2) {
            it_u->second.n_hadamard %= 2;
        }
    }

    if (it_u->second.empty()) {
        vertices[u].neighbors.erase(v);
        vertices[v].neighbors.erase(u);
    } else {
        vertices[v].neighbors[u] = it_u->second;
    }
}

void Graph::normalize_self_loops(VertexId v) {
    if (!is_valid_vertex(v)) return;

    // Simple self loop on Z spider: scalar 2
    if (vertices[v].self_simple > 0) {
        scalar.mul_two(vertices[v].self_simple);
        vertices[v].self_simple = 0;
    }

    // Hadamard self loop on Z spider: adds pi to phase!
    if (vertices[v].self_hadamard > 0) {
        if (vertices[v].self_hadamard % 2 != 0) {
            vertices[v].phase += Phase::pi();
        }
        vertices[v].self_hadamard = 0;
    }
}

bool Graph::validate() const {
    // 1. Check symmetry of adjacency
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (!vertices[i].active) continue;
        for (const auto& [nbr, count] : vertices[i].neighbors) {
            if (!is_valid_vertex(nbr)) return false;
            auto it = vertices[nbr].neighbors.find(static_cast<VertexId>(i));
            if (it == vertices[nbr].neighbors.end()) return false;
            if (it->second.n_simple != count.n_simple || it->second.n_hadamard != count.n_hadamard) {
                return false;
            }
        }
    }

    // 2. Check boundary vertices: degree 1, no self loops, phase 0
    for (VertexId b : inputs) {
        if (!is_valid_vertex(b) || vertices[b].type != VertexType::Boundary) return false;
        if (degree(b) != 1) return false;
        if (!vertices[b].phase.is_zero()) return false;
        if (vertices[b].self_simple != 0 || vertices[b].self_hadamard != 0) return false;
    }

    for (VertexId b : outputs) {
        if (!is_valid_vertex(b) || vertices[b].type != VertexType::Boundary) return false;
        if (degree(b) != 1) return false;
        if (!vertices[b].phase.is_zero()) return false;
        if (vertices[b].self_simple != 0 || vertices[b].self_hadamard != 0) return false;
    }

    return true;
}

bool Graph::is_graph_like() const {
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (!vertices[i].active) continue;
        if (vertices[i].type == VertexType::Boundary) continue;

        // Interior vertices must be Z spiders
        if (vertices[i].type != VertexType::Z) return false;

        // No self loops
        if (vertices[i].self_simple != 0 || vertices[i].self_hadamard != 0) return false;

        // Edges between interior vertices must be Hadamard only (n_simple == 0)
        for (const auto& [nbr, count] : vertices[i].neighbors) {
            if (vertices[nbr].type != VertexType::Boundary && count.n_simple != 0) {
                return false;
            }
        }
    }
    return true;
}

} // namespace zx
