#include "io/qasm_writer.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace zx {

std::string QasmWriter::to_string(const CircuitIR& ir) {
    std::ostringstream oss;
    oss << "OPENQASM 2.0;\n";
    oss << "include \"qelib1.inc\";\n";
    oss << "qreg q[" << ir.num_qubits << "];\n";

    for (const auto& g : ir.gates) {
        switch (g.type) {
            case GateType::H:
                oss << "h q[" << g.targets[0] << "];\n";
                break;
            case GateType::X:
                oss << "x q[" << g.targets[0] << "];\n";
                break;
            case GateType::Y:
                oss << "y q[" << g.targets[0] << "];\n";
                break;
            case GateType::Z:
                oss << "z q[" << g.targets[0] << "];\n";
                break;
            case GateType::S:
                oss << "s q[" << g.targets[0] << "];\n";
                break;
            case GateType::Sdg:
                oss << "sdg q[" << g.targets[0] << "];\n";
                break;
            case GateType::T:
                oss << "t q[" << g.targets[0] << "];\n";
                break;
            case GateType::Tdg:
                oss << "tdg q[" << g.targets[0] << "];\n";
                break;
            case GateType::Rz: {
                if (g.phase.is_zero()) {
                    // No-op
                } else if (g.phase == Phase::pi_over_4()) {
                    oss << "t q[" << g.targets[0] << "];\n";
                } else if (g.phase == Phase::minus_pi_over_4()) {
                    oss << "tdg q[" << g.targets[0] << "];\n";
                } else if (g.phase == Phase::half_pi()) {
                    oss << "s q[" << g.targets[0] << "];\n";
                } else if (g.phase == Phase::minus_half_pi()) {
                    oss << "sdg q[" << g.targets[0] << "];\n";
                } else if (g.phase.is_pauli()) {
                    oss << "z q[" << g.targets[0] << "];\n";
                } else {
                    oss << "rz(" << g.phase.to_string() << ") q[" << g.targets[0] << "];\n";
                }
                break;
            }
            case GateType::Rx: {
                if (g.phase.is_zero()) {
                    // No-op
                } else if (g.phase.is_pauli()) {
                    oss << "x q[" << g.targets[0] << "];\n";
                } else {
                    oss << "rx(" << g.phase.to_string() << ") q[" << g.targets[0] << "];\n";
                }
                break;
            }
            case GateType::CX:
                oss << "cx q[" << g.targets[0] << "],q[" << g.targets[1] << "];\n";
                break;
            case GateType::CZ:
                oss << "cz q[" << g.targets[0] << "],q[" << g.targets[1] << "];\n";
                break;
            case GateType::Swap:
                oss << "swap q[" << g.targets[0] << "],q[" << g.targets[1] << "];\n";
                break;
            case GateType::CCX:
                oss << "ccx q[" << g.targets[0] << "],q[" << g.targets[1] << "],q[" << g.targets[2] << "];\n";
                break;
            case GateType::CCZ:
                oss << "h q[" << g.targets[2] << "];\n";
                oss << "ccx q[" << g.targets[0] << "],q[" << g.targets[1] << "],q[" << g.targets[2] << "];\n";
                oss << "h q[" << g.targets[2] << "];\n";
                break;
        }
    }

    return oss.str();
}

void QasmWriter::to_file(const CircuitIR& ir, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not write QASM file: " + filepath);
    }
    file << to_string(ir);
}

} // namespace zx
