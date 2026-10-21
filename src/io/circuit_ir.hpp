#pragma once

#include "core/phase.hpp"
#include <string>
#include <vector>
#include <variant>

namespace zx {

enum class GateType {
    H,
    X,
    Y,
    Z,
    S,
    Sdg,
    T,
    Tdg,
    Rz,
    Rx,
    CX,
    CZ,
    Swap,
    CCX,
    CCZ
};

struct Gate {
    GateType type;
    std::vector<uint32_t> targets; // [target] or [ctrl, target] or [c1, c2, target]
    Phase phase;                   // For Rz, Rx

    static Gate make_h(uint32_t q) { return {GateType::H, {q}, Phase::zero()}; }
    static Gate make_x(uint32_t q) { return {GateType::X, {q}, Phase::zero()}; }
    static Gate make_y(uint32_t q) { return {GateType::Y, {q}, Phase::zero()}; }
    static Gate make_z(uint32_t q) { return {GateType::Z, {q}, Phase::zero()}; }
    static Gate make_s(uint32_t q) { return {GateType::S, {q}, Phase::zero()}; }
    static Gate make_sdg(uint32_t q) { return {GateType::Sdg, {q}, Phase::zero()}; }
    static Gate make_t(uint32_t q) { return {GateType::T, {q}, Phase::zero()}; }
    static Gate make_tdg(uint32_t q) { return {GateType::Tdg, {q}, Phase::zero()}; }
    static Gate make_rz(uint32_t q, Phase ph) { return {GateType::Rz, {q}, ph}; }
    static Gate make_rx(uint32_t q, Phase ph) { return {GateType::Rx, {q}, ph}; }
    static Gate make_cx(uint32_t ctrl, uint32_t tgt) { return {GateType::CX, {ctrl, tgt}, Phase::zero()}; }
    static Gate make_cz(uint32_t q1, uint32_t q2) { return {GateType::CZ, {q1, q2}, Phase::zero()}; }
    static Gate make_swap(uint32_t q1, uint32_t q2) { return {GateType::Swap, {q1, q2}, Phase::zero()}; }
    static Gate make_ccx(uint32_t c1, uint32_t c2, uint32_t tgt) { return {GateType::CCX, {c1, c2, tgt}, Phase::zero()}; }
};

struct CircuitIR {
    uint32_t num_qubits = 0;
    std::vector<Gate> gates;

    void add_gate(Gate g) {
        for (uint32_t q : g.targets) {
            if (q >= num_qubits) {
                num_qubits = q + 1;
            }
        }
        gates.push_back(std::move(g));
    }

    [[nodiscard]] size_t t_count() const noexcept {
        size_t count = 0;
        for (const auto& g : gates) {
            if (g.type == GateType::T || g.type == GateType::Tdg) {
                count++;
            } else if (g.type == GateType::Rz && g.phase.is_t_like()) {
                count++;
            } else if (g.type == GateType::CCX || g.type == GateType::CCZ) {
                count += 7; // standard Toffoli T-count
            }
        }
        return count;
    }

    [[nodiscard]] size_t cnot_count() const noexcept {
        size_t count = 0;
        for (const auto& g : gates) {
            if (g.type == GateType::CX) {
                count++;
            } else if (g.type == GateType::CZ) {
                count++; // or count as 1 two-qubit gate
            } else if (g.type == GateType::Swap) {
                count += 3; // 3 CNOTs for SWAP
            } else if (g.type == GateType::CCX || g.type == GateType::CCZ) {
                count += 6; // 6 CNOTs for standard Toffoli
            }
        }
        return count;
    }

    [[nodiscard]] size_t total_gates() const noexcept {
        return gates.size();
    }
};

} // namespace zx
