#include "zx/extract.hpp"
#include "core/bitmatrix.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <unordered_set>
#include <iostream>

namespace zx {

CircuitIR CircuitExtractor::extract(Graph g) {
    CircuitIR ir;
    uint32_t n = static_cast<uint32_t>(g.outputs.size());
    ir.num_qubits = n;
    if (n == 0) return ir;

    std::vector<VertexId> frontier;
    std::unordered_map<VertexId, uint32_t> qubit_map;
    std::vector<Gate> extracted_gates;

    auto emit_rz = [&](uint32_t q, Phase ph) {
        if (!ph.is_zero()) {
            extracted_gates.push_back(Gate::make_rz(q, ph));
        }
    };

    auto emit_cz = [&](uint32_t q1, uint32_t q2) {
        extracted_gates.push_back(Gate::make_cz(q1, q2));
    };

    auto emit_cx = [&](uint32_t ctrl, uint32_t tgt) {
        extracted_gates.push_back(Gate::make_cx(ctrl, tgt));
    };

    auto emit_h = [&](uint32_t q) {
        extracted_gates.push_back(Gate::make_h(q));
    };

    std::unordered_set<VertexId> input_set(g.inputs.begin(), g.inputs.end());
    std::unordered_set<VertexId> output_set(g.outputs.begin(), g.outputs.end());

    // 1. Initialize frontier: for each output boundary, pick its internal neighbor
    for (uint32_t q = 0; q < n; ++q) {
        VertexId out_b = g.outputs[q];
        auto nbrs = g.neighbors(out_b);
        if (nbrs.empty()) {
            throw std::runtime_error("Disconnected output boundary at qubit " + std::to_string(q));
        }
        VertexId v = nbrs[0];
        if (input_set.count(v)) {
            continue; // Already connected directly to an input
        }
        frontier.push_back(v);
        qubit_map[v] = q;
    }

    size_t max_steps = 20000;
    size_t step = 0;

    while (!frontier.empty() && step++ < max_steps) {
        // Step A: Clean frontier: remove Hadamard edges to outputs, phases, and CZs
        for (VertexId v : frontier) {
            uint32_t q = qubit_map[v];
            // Find connected output boundary
            VertexId out_b = INVALID_VERTEX;
            for (VertexId nbr : g.neighbors(v)) {
                if (output_set.count(nbr)) {
                    out_b = nbr;
                    break;
                }
            }
            if (out_b != INVALID_VERTEX) {
                auto ec = g.edge_count(v, out_b);
                if (ec.n_hadamard > 0) {
                    emit_h(q);
                    g.remove_edge(v, out_b, EdgeType::Hadamard);
                    g.add_edge(v, out_b, EdgeType::Simple);
                }
            }
            if (!g.vertices[v].phase.is_zero()) {
                emit_rz(q, g.vertices[v].phase);
                g.vertices[v].phase = Phase::zero();
            }
        }

        // Emit CZ gates between frontier vertices
        for (size_t i = 0; i < frontier.size(); ++i) {
            for (size_t j = i + 1; j < frontier.size(); ++j) {
                VertexId v1 = frontier[i];
                VertexId v2 = frontier[j];
                auto ec = g.edge_count(v1, v2);
                if (ec.n_hadamard > 0) {
                    emit_cz(qubit_map[v1], qubit_map[v2]);
                    g.remove_edge(v1, v2, EdgeType::Hadamard);
                }
            }
        }

        // Step B: Manage connections to inputs and collect non-output neighbors
        std::unordered_set<VertexId> frontier_set(frontier.begin(), frontier.end());
        std::vector<VertexId> new_frontier;
        std::unordered_set<VertexId> neighbor_set;

        for (VertexId v : frontier) {
            std::vector<VertexId> non_outputs;
            for (VertexId w : g.neighbors(v)) {
                if (!output_set.count(w)) {
                    non_outputs.push_back(w);
                }
            }

            // Check if v connects to an input
            VertexId in_connected = INVALID_VERTEX;
            for (VertexId w : non_outputs) {
                if (input_set.count(w)) {
                    in_connected = w;
                    break;
                }
            }

            if (in_connected != INVALID_VERTEX) {
                if (non_outputs.size() == 1) {
                    // v is only connected to this input and the output: finished!
                    continue; // do not include in new_frontier
                }
                // Disconnect v from input by inserting a new Z-spider w
                auto ec = g.edge_count(v, in_connected);
                EdgeType et = (ec.n_hadamard > 0) ? EdgeType::Hadamard : EdgeType::Simple;
                g.remove_edge(v, in_connected, et);

                VertexId new_spider = g.add_vertex(VertexType::Z, Phase::zero(), g.vertices[v].qubit, 0);
                g.add_edge(v, new_spider, EdgeType::Hadamard);
                EdgeType toggled_et = (et == EdgeType::Hadamard) ? EdgeType::Simple : EdgeType::Hadamard;
                g.add_edge(new_spider, in_connected, toggled_et);

                non_outputs.erase(std::remove(non_outputs.begin(), non_outputs.end(), in_connected), non_outputs.end());
                non_outputs.push_back(new_spider);
            }

            new_frontier.push_back(v);
            for (VertexId w : non_outputs) {
                if (!frontier_set.count(w)) {
                    neighbor_set.insert(w);
                }
            }
        }

        frontier = std::move(new_frontier);
        if (frontier.empty()) {
            break;
        }

        std::vector<VertexId> neighbors(neighbor_set.begin(), neighbor_set.end());
        size_t n_rows = frontier.size();
        size_t n_cols = neighbors.size();

        // Step C: Construct biadjacency matrix (rows = frontier, cols = neighbors)
        BitMatrix m(n_rows, n_cols);
        for (size_t r = 0; r < n_rows; ++r) {
            for (size_t c = 0; c < n_cols; ++c) {
                if (g.has_edge(frontier[r], neighbors[c])) {
                    m.set(r, c, true);
                }
            }
        }

        // Step D: Look for frontier vertex with degree 1 into neighbors
        bool extracted = false;
        for (size_t r = 0; r < n_rows; ++r) {
            if (m.row_popcount(r) == 1) {
                size_t c = 0;
                for (size_t col_k = 0; col_k < n_cols; ++col_k) {
                    if (m.get(r, col_k)) {
                        c = col_k;
                        break;
                    }
                }
                VertexId v = frontier[r];
                VertexId w = neighbors[c];
                uint32_t q = qubit_map[v];

                // Emit H on q
                emit_h(q);

                // Find output boundary attached to v
                VertexId out_b = INVALID_VERTEX;
                for (VertexId nbr : g.neighbors(v)) {
                    if (output_set.count(nbr)) {
                        out_b = nbr;
                        break;
                    }
                }

                // Transfer frontier status to w
                qubit_map[w] = q;
                g.remove_vertex(v);
                if (out_b != INVALID_VERTEX) {
                    g.add_edge(w, out_b, EdgeType::Simple);
                }
                frontier[r] = w;
                extracted = true;
                break;
            }
        }

        if (extracted) {
            continue;
        }

        // Step E: Greedy CNOT reduction / Gaussian elimination on rows of m
        bool cnot_applied = false;
        size_t best_reduction = 0;
        size_t best_src = 0, best_dst = 0;

        for (size_t r1 = 0; r1 < n_rows; ++r1) {
            for (size_t r2 = 0; r2 < n_rows; ++r2) {
                if (r1 == r2) continue;
                size_t w2 = m.row_popcount(r2);
                size_t xor_w = 0;
                for (size_t c = 0; c < n_cols; ++c) {
                    if (m.get(r2, c) ^ m.get(r1, c)) xor_w++;
                }
                if (xor_w < w2) {
                    size_t red = w2 - xor_w;
                    if (red > best_reduction) {
                        best_reduction = red;
                        best_src = r1;
                        best_dst = r2;
                    }
                }
            }
        }

        if (best_reduction > 0) {
            uint32_t ctrl_q = qubit_map[frontier[best_dst]];
            uint32_t tgt_q = qubit_map[frontier[best_src]];
            emit_cx(ctrl_q, tgt_q);

            VertexId v_tgt = frontier[best_dst];
            for (size_t c = 0; c < n_cols; ++c) {
                bool old_b = m.get(best_dst, c);
                bool new_b = old_b ^ m.get(best_src, c);
                if (new_b != old_b) {
                    m.set(best_dst, c, new_b);
                    if (new_b) {
                        g.add_edge(v_tgt, neighbors[c], EdgeType::Hadamard);
                    } else {
                        g.remove_edge(v_tgt, neighbors[c], EdgeType::Hadamard);
                    }
                }
            }
            cnot_applied = true;
        } else {
            // Elimination to produce a weight-1 row
            for (size_t c = 0; c < n_cols; ++c) {
                size_t pivot_r = n_rows;
                for (size_t r = 0; r < n_rows; ++r) {
                    if (m.get(r, c)) {
                        pivot_r = r;
                        break;
                    }
                }
                if (pivot_r < n_rows) {
                    for (size_t r = 0; r < n_rows; ++r) {
                        if (r != pivot_r && m.get(r, c)) {
                            uint32_t ctrl_q = qubit_map[frontier[r]];
                            uint32_t tgt_q = qubit_map[frontier[pivot_r]];
                            emit_cx(ctrl_q, tgt_q);

                            VertexId v_tgt = frontier[r];
                            for (size_t col_k = 0; col_k < n_cols; ++col_k) {
                                bool old_b = m.get(r, col_k);
                                bool new_b = old_b ^ m.get(pivot_r, col_k);
                                if (new_b != old_b) {
                                    m.set(r, col_k, new_b);
                                    if (new_b) {
                                        g.add_edge(v_tgt, neighbors[col_k], EdgeType::Hadamard);
                                    } else {
                                        g.remove_edge(v_tgt, neighbors[col_k], EdgeType::Hadamard);
                                    }
                                }
                            }
                            cnot_applied = true;
                            break;
                        }
                    }
                }
                if (cnot_applied) break;
            }
        }

        if (!cnot_applied) {
            break;
        }
    }

    // Step F: Cleanup identity spiders between inputs and outputs
    for (uint32_t q = 0; q < n; ++q) {
        VertexId out_b = g.outputs[q];
        auto nbrs = g.neighbors(out_b);
        if (!nbrs.empty()) {
            VertexId u = nbrs[0];
            if (!input_set.count(u)) {
                auto u_nbrs = g.neighbors(u);
                for (VertexId in_b : u_nbrs) {
                    if (input_set.count(in_b)) {
                        auto ec1 = g.edge_count(in_b, u);
                        auto ec2 = g.edge_count(u, out_b);
                        bool is_h1 = ec1.n_hadamard > 0;
                        bool is_h2 = ec2.n_hadamard > 0;
                        EdgeType final_et = (is_h1 ^ is_h2) ? EdgeType::Hadamard : EdgeType::Simple;

                        g.remove_vertex(u);
                        g.add_edge(in_b, out_b, final_et);
                        break;
                    }
                }
            }
        }
    }

    // Check remaining Hadamard edges between inputs and outputs
    for (uint32_t q = 0; q < n; ++q) {
        VertexId out_b = g.outputs[q];
        auto nbrs = g.neighbors(out_b);
        if (!nbrs.empty()) {
            VertexId in_b = nbrs[0];
            auto ec = g.edge_count(in_b, out_b);
            if (ec.n_hadamard > 0) {
                emit_h(q);
            }
        }
    }

    // Check permutations and decompose into swaps
    std::vector<uint32_t> perm(n);
    for (uint32_t q = 0; q < n; ++q) {
        perm[q] = q;
        VertexId out_b = g.outputs[q];
        auto nbrs = g.neighbors(out_b);
        if (!nbrs.empty()) {
            VertexId in_b = nbrs[0];
            for (uint32_t in_q = 0; in_q < n; ++in_q) {
                if (g.inputs[in_q] == in_b) {
                    perm[q] = in_q;
                    break;
                }
            }
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> swaps;
    std::vector<uint32_t> l = perm;
    std::vector<uint32_t> linv(n);
    for (uint32_t i = 0; i < n; ++i) {
        linv[perm[i]] = i;
    }
    for (uint32_t i = 0; i < n; ++i) {
        if (l[i] == i) continue;
        uint32_t t1 = l[i];
        uint32_t t2 = linv[i];
        swaps.push_back({i, t2});
        l[t2] = t1;
        linv[t1] = t2;
    }

    for (const auto& [t1, t2] : swaps) {
        emit_cx(t1, t2);
        emit_cx(t2, t1);
        emit_cx(t1, t2);
    }

    // Step G: Reverse the extracted gates to form forward circuit
    std::reverse(extracted_gates.begin(), extracted_gates.end());
    for (auto& gate : extracted_gates) {
        ir.add_gate(gate);
    }

    return ir;
}

} // namespace zx
