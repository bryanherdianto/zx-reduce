#include "zx/from_circuit.hpp"

namespace zx {

Graph CircuitToGraph::convert(const CircuitIR& ir) {
    Graph g;
    uint32_t n = ir.num_qubits;
    if (n == 0) return g;

    std::vector<VertexId> cur(n);
    int32_t current_row = 0;

    // Create inputs
    for (uint32_t q = 0; q < n; ++q) {
        auto in_v = g.add_vertex(VertexType::Boundary, Phase::zero(), static_cast<int32_t>(q), current_row);
        g.inputs.push_back(in_v);
        cur[q] = in_v;
    }
    current_row++;

    auto emit_toffoli = [&](uint32_t c1, uint32_t c2, uint32_t tgt, auto& emit_ref) -> void {
        // Standard 7-T Clifford+T decomposition
        CircuitIR dec;
        dec.num_qubits = n;
        dec.add_gate(Gate::make_h(tgt));
        dec.add_gate(Gate::make_cx(c2, tgt));
        dec.add_gate(Gate::make_tdg(tgt));
        dec.add_gate(Gate::make_cx(c1, tgt));
        dec.add_gate(Gate::make_t(tgt));
        dec.add_gate(Gate::make_cx(c2, tgt));
        dec.add_gate(Gate::make_t(c2));
        dec.add_gate(Gate::make_tdg(tgt));
        dec.add_gate(Gate::make_cx(c1, tgt));
        dec.add_gate(Gate::make_cx(c1, c2));
        dec.add_gate(Gate::make_t(c1));
        dec.add_gate(Gate::make_tdg(c2));
        dec.add_gate(Gate::make_cx(c1, c2));
        dec.add_gate(Gate::make_t(tgt));
        dec.add_gate(Gate::make_h(tgt));

        for (const auto& gate : dec.gates) {
            emit_ref(gate);
        }
    };

    auto emit_gate = [&](const Gate& gate) -> void {
        switch (gate.type) {
            case GateType::H: {
                uint32_t q = gate.targets[0];
                auto h_v = g.add_vertex(VertexType::Z, Phase::zero(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], h_v, EdgeType::Hadamard);
                cur[q] = h_v;
                break;
            }
            case GateType::X: {
                uint32_t q = gate.targets[0];
                auto x_v = g.add_vertex(VertexType::X, Phase::pi(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], x_v, EdgeType::Simple);
                cur[q] = x_v;
                break;
            }
            case GateType::Y: {
                // Y = Z * X (up to scalar i)
                uint32_t q = gate.targets[0];
                auto z_v = g.add_vertex(VertexType::Z, Phase::pi(), static_cast<int32_t>(q), current_row);
                auto x_v = g.add_vertex(VertexType::X, Phase::pi(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], z_v, EdgeType::Simple);
                g.add_edge(z_v, x_v, EdgeType::Simple);
                cur[q] = x_v;
                break;
            }
            case GateType::Z: {
                uint32_t q = gate.targets[0];
                auto z_v = g.add_vertex(VertexType::Z, Phase::pi(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], z_v, EdgeType::Simple);
                cur[q] = z_v;
                break;
            }
            case GateType::S: {
                uint32_t q = gate.targets[0];
                auto z_v = g.add_vertex(VertexType::Z, Phase::half_pi(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], z_v, EdgeType::Simple);
                cur[q] = z_v;
                break;
            }
            case GateType::Sdg: {
                uint32_t q = gate.targets[0];
                auto z_v = g.add_vertex(VertexType::Z, Phase::minus_half_pi(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], z_v, EdgeType::Simple);
                cur[q] = z_v;
                break;
            }
            case GateType::T: {
                uint32_t q = gate.targets[0];
                auto z_v = g.add_vertex(VertexType::Z, Phase::pi_over_4(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], z_v, EdgeType::Simple);
                cur[q] = z_v;
                break;
            }
            case GateType::Tdg: {
                uint32_t q = gate.targets[0];
                auto z_v = g.add_vertex(VertexType::Z, Phase::minus_pi_over_4(), static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], z_v, EdgeType::Simple);
                cur[q] = z_v;
                break;
            }
            case GateType::Rz: {
                uint32_t q = gate.targets[0];
                auto z_v = g.add_vertex(VertexType::Z, gate.phase, static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], z_v, EdgeType::Simple);
                cur[q] = z_v;
                break;
            }
            case GateType::Rx: {
                uint32_t q = gate.targets[0];
                auto x_v = g.add_vertex(VertexType::X, gate.phase, static_cast<int32_t>(q), current_row);
                g.add_edge(cur[q], x_v, EdgeType::Simple);
                cur[q] = x_v;
                break;
            }
            case GateType::CX: {
                uint32_t c = gate.targets[0];
                uint32_t t = gate.targets[1];
                auto z_c = g.add_vertex(VertexType::Z, Phase::zero(), static_cast<int32_t>(c), current_row);
                auto x_t = g.add_vertex(VertexType::X, Phase::zero(), static_cast<int32_t>(t), current_row);

                g.add_edge(cur[c], z_c, EdgeType::Simple);
                g.add_edge(cur[t], x_t, EdgeType::Simple);
                g.add_edge(z_c, x_t, EdgeType::Simple);

                cur[c] = z_c;
                cur[t] = x_t;
                break;
            }
            case GateType::CZ: {
                uint32_t q1 = gate.targets[0];
                uint32_t q2 = gate.targets[1];
                auto z1 = g.add_vertex(VertexType::Z, Phase::zero(), static_cast<int32_t>(q1), current_row);
                auto z2 = g.add_vertex(VertexType::Z, Phase::zero(), static_cast<int32_t>(q2), current_row);

                g.add_edge(cur[q1], z1, EdgeType::Simple);
                g.add_edge(cur[q2], z2, EdgeType::Simple);
                g.add_edge(z1, z2, EdgeType::Hadamard);

                cur[q1] = z1;
                cur[q2] = z2;
                break;
            }
            case GateType::Swap: {
                uint32_t q1 = gate.targets[0];
                uint32_t q2 = gate.targets[1];
                std::swap(cur[q1], cur[q2]);
                break;
            }
            case GateType::CCX: {
                // Handled via decomposition helper
                break;
            }
            case GateType::CCZ: {
                break;
            }
        }
        current_row++;
    };

    for (const auto& gate : ir.gates) {
        if (gate.type == GateType::CCX) {
            emit_toffoli(gate.targets[0], gate.targets[1], gate.targets[2], emit_gate);
        } else if (gate.type == GateType::CCZ) {
            emit_gate(Gate::make_h(gate.targets[2]));
            emit_toffoli(gate.targets[0], gate.targets[1], gate.targets[2], emit_gate);
            emit_gate(Gate::make_h(gate.targets[2]));
        } else {
            emit_gate(gate);
        }
    }

    // Connect to outputs
    for (uint32_t q = 0; q < n; ++q) {
        auto out_v = g.add_vertex(VertexType::Boundary, Phase::zero(), static_cast<int32_t>(q), current_row);
        g.add_edge(cur[q], out_v, EdgeType::Simple);
        g.outputs.push_back(out_v);
    }

    return g;
}

} // namespace zx
