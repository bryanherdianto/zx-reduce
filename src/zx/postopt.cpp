#include "zx/postopt.hpp"

namespace zx {

CircuitIR PostOptimizer::optimize(const CircuitIR& ir) {
    CircuitIR opt;
    opt.num_qubits = ir.num_qubits;

    std::vector<Gate> current = ir.gates;
    bool changed = true;

    while (changed) {
        changed = false;
        std::vector<Gate> next;

        for (size_t i = 0; i < current.size(); ++i) {
            if (i + 1 < current.size()) {
                const auto& g1 = current[i];
                const auto& g2 = current[i + 1];

                // 1. Two consecutive H gates on same qubit: H * H = I
                if (g1.type == GateType::H && g2.type == GateType::H && g1.targets[0] == g2.targets[0]) {
                    i++; // skip both
                    changed = true;
                    continue;
                }

                // 2. Two consecutive identical CX gates: CX * CX = I
                if (g1.type == GateType::CX && g2.type == GateType::CX &&
                    g1.targets[0] == g2.targets[0] && g1.targets[1] == g2.targets[1]) {
                    i++; // skip both
                    changed = true;
                    continue;
                }

                // 3. Two consecutive identical CZ gates: CZ * CZ = I
                if (g1.type == GateType::CZ && g2.type == GateType::CZ &&
                    ((g1.targets[0] == g2.targets[0] && g1.targets[1] == g2.targets[1]) ||
                     (g1.targets[0] == g2.targets[1] && g1.targets[1] == g2.targets[0]))) {
                    i++; // skip both
                    changed = true;
                    continue;
                }

                // 4. Merge adjacent Rz rotations: Rz(a) * Rz(b) = Rz(a + b)
                if (g1.type == GateType::Rz && g2.type == GateType::Rz && g1.targets[0] == g2.targets[0]) {
                    Phase sum = g1.phase + g2.phase;
                    if (!sum.is_zero()) {
                        next.push_back(Gate::make_rz(g1.targets[0], sum));
                    }
                    i++; // consumed both
                    changed = true;
                    continue;
                }

                // 5. Merge adjacent Rx rotations: Rx(a) * Rx(b) = Rx(a + b)
                if (g1.type == GateType::Rx && g2.type == GateType::Rx && g1.targets[0] == g2.targets[0]) {
                    Phase sum = g1.phase + g2.phase;
                    if (!sum.is_zero()) {
                        next.push_back(Gate::make_rx(g1.targets[0], sum));
                    }
                    i++; // consumed both
                    changed = true;
                    continue;
                }
            }

            // Keep gate if not zero-angle rotation
            if (current[i].type == GateType::Rz && current[i].phase.is_zero()) {
                changed = true;
                continue;
            }
            if (current[i].type == GateType::Rx && current[i].phase.is_zero()) {
                changed = true;
                continue;
            }

            next.push_back(current[i]);
        }

        current = std::move(next);
    }

    opt.gates = std::move(current);
    return opt;
}

} // namespace zx
