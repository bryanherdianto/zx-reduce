#include "io/qasm_parser.hpp"
#include "io/qasm_writer.hpp"
#include "zx/from_circuit.hpp"
#include "zx/to_graph_like.hpp"
#include "zx/simplify.hpp"
#include "zx/extract.hpp"
#include "zx/postopt.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cstdlib>
#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: zx-reduce <input.qasm> [-o <output.qasm>] [--stats <stats.json>] [--verify] [--clifford-only] [--quiet]\n";
        return 1;
    }

    std::string input_path;
    std::string output_path;
    std::string stats_path;
    bool verify = false;
    bool clifford_only = false;
    bool quiet = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--stats" && i + 1 < argc) {
            stats_path = argv[++i];
        } else if (arg == "--verify") {
            verify = true;
        } else if (arg == "--clifford-only") {
            clifford_only = true;
        } else if (arg == "--quiet") {
            quiet = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "zx-reduce: High-Performance C++20 ZX-Calculus Quantum Circuit Optimizer\n\n"
                      << "Usage:\n"
                      << "  zx-reduce <input.qasm> [options]\n\n"
                      << "Options:\n"
                      << "  -o <path>           Output QASM file path\n"
                      << "  --stats <path>      Output JSON file with detailed metrics\n"
                      << "  --verify            Formally verify equivalence via qdd-sim\n"
                      << "  --clifford-only     Run Clifford rewrites only (no gadget fusion)\n"
                      << "  --quiet             Suppress standard log output\n"
                      << "  --help, -h          Show this help message\n";
            return 0;
        } else if (input_path.empty()) {
            input_path = arg;
        }
    }

    if (input_path.empty()) {
        std::cerr << "Error: No input QASM file specified.\n";
        return 1;
    }

    auto t0 = std::chrono::steady_clock::now();

    // 1. Parse Input Circuit
    zx::CircuitIR ir_in;
    try {
        ir_in = zx::QasmParser::from_file(input_path);
    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: Failed to parse QASM: " << e.what() << "\n";
        return 1;
    }
    auto t1 = std::chrono::steady_clock::now();

    size_t t_before = ir_in.t_count();
    size_t cx_before = ir_in.cnot_count();
    size_t gates_before = ir_in.total_gates();

    if (!quiet) {
        std::cout << "Loaded " << input_path << ": " << ir_in.num_qubits << " qubits, "
                  << gates_before << " gates (T-count: " << t_before << ", CNOTs: " << cx_before << ")\n";
    }

    // 2. Convert to ZX Graph
    zx::Graph g = zx::CircuitToGraph::convert(ir_in);
    auto t2 = std::chrono::steady_clock::now();

    // 3. Simplify
    if (clifford_only) {
        zx::Simplifier::clifford_reduce(g);
    } else {
        zx::Simplifier::full_reduce(g);
    }
    auto t3 = std::chrono::steady_clock::now();

    // 4. Extract Circuit
    zx::CircuitIR ir_extracted;
    try {
        ir_extracted = zx::CircuitExtractor::extract(g);
    } catch (const std::exception& e) {
        std::cerr << "Extraction Failure: " << e.what() << "\n";
        return 1;
    }
    auto t4 = std::chrono::steady_clock::now();

    // 5. Post-Optimization
    zx::CircuitIR ir_opt = zx::PostOptimizer::optimize(ir_extracted);
    auto t5 = std::chrono::steady_clock::now();

    size_t t_after = ir_opt.t_count();
    size_t cx_after = ir_opt.cnot_count();
    size_t gates_after = ir_opt.total_gates();

    double parse_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double to_graph_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    double simplify_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();
    double extract_ms = std::chrono::duration<double, std::milli>(t4 - t3).count();
    double postopt_ms = std::chrono::duration<double, std::milli>(t5 - t4).count();
    double total_ms = std::chrono::duration<double, std::milli>(t5 - t0).count();

    if (!quiet) {
        std::cout << "Optimized: " << gates_after << " gates (T-count: " << t_after
                  << " [-" << (t_before > t_after ? t_before - t_after : 0) << "]"
                  << ", CNOTs: " << cx_after << ")\n"
                  << "Execution Time: " << total_ms << " ms ("
                  << "simplify: " << simplify_ms << " ms, extract: " << extract_ms << " ms)\n";
    }

    // 6. Output QASM
    std::string out_qasm_text = zx::QasmWriter::to_string(ir_opt);
    if (!output_path.empty()) {
        zx::QasmWriter::to_file(ir_opt, output_path);
        if (!quiet) {
            std::cout << "Wrote optimized circuit to " << output_path << "\n";
        }
    } else {
        std::cout << out_qasm_text;
    }

    // 7. Write JSON stats if requested
    if (!stats_path.empty()) {
        std::ofstream sf(stats_path);
        if (sf.is_open()) {
            sf << "{\n"
               << "  \"num_qubits\": " << ir_in.num_qubits << ",\n"
               << "  \"t_count_before\": " << t_before << ",\n"
               << "  \"t_count_after\": " << t_after << ",\n"
               << "  \"cnot_count_before\": " << cx_before << ",\n"
               << "  \"cnot_count_after\": " << cx_after << ",\n"
               << "  \"total_gates_before\": " << gates_before << ",\n"
               << "  \"total_gates_after\": " << gates_after << ",\n"
               << "  \"parse_time_ms\": " << parse_ms << ",\n"
               << "  \"to_graph_time_ms\": " << to_graph_ms << ",\n"
               << "  \"simplify_time_ms\": " << simplify_ms << ",\n"
               << "  \"extract_time_ms\": " << extract_ms << ",\n"
               << "  \"postopt_time_ms\": " << postopt_ms << ",\n"
               << "  \"total_time_ms\": " << total_ms << "\n"
               << "}\n";
        }
    }

    // 8. Verification via qdd-sim
    if (verify) {
        if (!quiet) {
            std::cout << "Formally verifying equivalence via qdd-sim...\n";
        }
        std::string verify_out = output_path;
        if (verify_out.empty()) {
            verify_out = "temp_verify_out.qasm";
            zx::QasmWriter::to_file(ir_opt, verify_out);
        }

        // Auto-locate qdd-sim executable
        std::vector<std::string> search_paths = {
            "qdd-sim",
            "qdd-sim.exe",
            "../qdd-sim/build/release/qdd-sim.exe",
            "../../qdd-sim/build/release/qdd-sim.exe",
            "C:/Users/victo/Documents/tiny-bdd-cec/qdd-sim/build/release/qdd-sim.exe",
            "C:/Users/victo/Documents/qdd-sim/build/release/qdd-sim.exe"
        };

        std::string qdd_sim_bin;
        for (const auto& p : search_paths) {
            if (p == "qdd-sim" || p == "qdd-sim.exe") {
                // Check if in PATH via system command
                continue;
            }
            if (std::filesystem::exists(p)) {
                qdd_sim_bin = p;
                break;
            }
        }
        if (qdd_sim_bin.empty()) {
            qdd_sim_bin = "qdd-sim";
        }

        std::string qdd_sim_cmd = "powershell -Command \"& '" + qdd_sim_bin + "' --ec '" + input_path + "' '" + verify_out + "'\"";
        int ret = std::system(qdd_sim_cmd.c_str());
        if (ret != 0) {
            std::cerr << "VERIFICATION FAILED: qdd-sim reported non-equivalence or error!\n";
            return 1;
        } else {
            if (!quiet) {
                std::cout << "VERIFICATION PASSED: Circuit equivalence proven up to global phase.\n";
            }
        }
    }

    return 0;
}
