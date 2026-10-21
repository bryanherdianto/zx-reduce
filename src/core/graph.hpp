#pragma once

#include "core/phase.hpp"
#include "core/scalar.hpp"
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <map>
#include <optional>
#include <stdexcept>
#include <cassert>

namespace zx {

using VertexId = uint32_t;
constexpr VertexId INVALID_VERTEX = UINT32_MAX;

enum class VertexType : uint8_t {
    Boundary,
    Z,
    X
};

enum class EdgeType : uint8_t {
    Simple,
    Hadamard
};

struct EdgeCount {
    uint8_t n_simple = 0;
    uint8_t n_hadamard = 0;

    constexpr bool empty() const noexcept {
        return n_simple == 0 && n_hadamard == 0;
    }
};

struct VertexData {
    VertexType type = VertexType::Z;
    Phase phase;
    int32_t qubit = -1;  // Qubit index for boundary/layout
    int32_t row = -1;    // Row index for layout
    bool active = false; // In-use slot indicator

    uint8_t self_simple = 0;
    uint8_t self_hadamard = 0;

    // Ordered map neighbor -> edge counts for deterministic iteration & stability
    std::map<VertexId, EdgeCount> neighbors;
};

/**
 * @brief Multigraph representation of a ZX diagram.
 * Stores vertices in a slot vector with free list.
 */
class Graph {
public:
    std::vector<VertexData> vertices;
    std::vector<VertexId> free_list;
    std::vector<VertexId> inputs;
    std::vector<VertexId> outputs;
    Scalar scalar;

    Graph() = default;

    VertexId add_vertex(VertexType type, Phase phase = Phase::zero(), int32_t qubit = -1, int32_t row = -1);
    void remove_vertex(VertexId v);

    [[nodiscard]] bool is_valid_vertex(VertexId v) const noexcept {
        return v < vertices.size() && vertices[v].active;
    }

    [[nodiscard]] size_t num_vertices() const noexcept {
        return vertices.size() - free_list.size();
    }

    void add_edge(VertexId u, VertexId v, EdgeType type = EdgeType::Simple);
    void remove_edge(VertexId u, VertexId v, EdgeType type);
    void set_edge_count(VertexId u, VertexId v, EdgeCount ec);

    [[nodiscard]] EdgeCount edge_count(VertexId u, VertexId v) const;
    [[nodiscard]] bool has_edge(VertexId u, VertexId v) const;

    [[nodiscard]] size_t degree(VertexId v) const;
    [[nodiscard]] std::vector<VertexId> neighbors(VertexId v) const;

    // Multigraph normalization: applies Hopf rules and self-loop reductions
    void normalize_edges(VertexId u, VertexId v);
    void normalize_self_loops(VertexId v);

    // Canonicity validation
    [[nodiscard]] bool validate() const;

    // Checks if graph is in graph-like form: all interior vertices Z, all interior edges Hadamard
    [[nodiscard]] bool is_graph_like() const;
};

} // namespace zx
