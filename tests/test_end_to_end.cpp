#include <catch2/catch_test_macros.hpp>
#include "io/qasm_parser.hpp"
#include "io/qasm_writer.hpp"
#include "zx/from_circuit.hpp"
#include "zx/to_graph_like.hpp"
#include "zx/simplify.hpp"
#include "zx/extract.hpp"
#include "zx/postopt.hpp"
#include "core/tensor_oracle.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace zx;

TEST_CASE("End-to-End ZX Optimization Pipeline", "[e2e]") {
    SECTION("Small Clifford+T Circuit with TensorOracle") {
        std::string qasm =
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[2];\n"
            "h q[0];\n"
            "cx q[0],q[1];\n"
            "t q[1];\n"
            "cx q[0],q[1];\n"
            "h q[0];\n";

        auto ir = QasmParser::from_string(qasm);
        auto g = CircuitToGraph::convert(ir);
        ToGraphLike::convert(g);
        auto mat_before = TensorOracle::to_matrix(g);

        Simplifier::full_reduce(g);
        std::cout << "=== Graph after Simplifier::full_reduce ===\n";
        for (size_t i = 0; i < g.vertices.size(); ++i) {
            if (!g.vertices[i].active) continue;
            std::cout << "v=" << i << ", type=" << static_cast<int>(g.vertices[i].type)
                      << ", qubit=" << g.vertices[i].qubit
                      << ", phase=" << g.vertices[i].phase.to_string()
                      << ", nbrs=";
            for (auto nbr : g.neighbors(static_cast<VertexId>(i))) {
                auto ec = g.edge_count(static_cast<VertexId>(i), nbr);
                std::cout << nbr << "(" << (ec.n_hadamard > 0 ? "H" : "S") << ") ";
            }
            std::cout << "\n";
        }
        auto ir_extracted = CircuitExtractor::extract(g);
        auto ir_opt = PostOptimizer::optimize(ir_extracted);

        std::cout << "Optimized small circuit:\n" << QasmWriter::to_string(ir_opt) << "\n";

        auto g_after = CircuitToGraph::convert(ir_opt);
        ToGraphLike::convert(g_after);
        auto mat_after = TensorOracle::to_matrix(g_after);
        for (size_t r = 0; r < mat_before.size(); ++r) {
            for (size_t c = 0; c < mat_before[r].size(); ++c) {
                if (std::abs(mat_before[r][c]) > 1e-4 || std::abs(mat_after[r][c]) > 1e-4) {
                    std::cout << "before[" << r << "][" << c << "]=" << mat_before[r][c]
                              << ", after=" << mat_after[r][c] << "\n";
                }
            }
        }
        REQUIRE(TensorOracle::are_equal_up_to_global_phase(mat_before, mat_after));
    }

    SECTION("Toffoli Gate Verification via qdd-sim") {
        std::string qasm =
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[3];\n"
            "ccx q[0],q[1],q[2];\n";

        auto ir = QasmParser::from_string(qasm);
        REQUIRE(ir.t_count() == 7);

        auto g = CircuitToGraph::convert(ir);
        Simplifier::full_reduce(g);
        auto ir_extracted = CircuitExtractor::extract(g);
        auto ir_opt = PostOptimizer::optimize(ir_extracted);

        std::cout << "Optimized Toffoli gates: " << ir_opt.gates.size() << ", T-count: " << ir_opt.t_count() << "\n";

        std::string in_path = "temp_e2e_tof_in.qasm";
        std::string out_path = "temp_e2e_tof_out.qasm";
        QasmWriter::to_file(ir, in_path);
        QasmWriter::to_file(ir_opt, out_path);

        std::string qdd_sim_bin = "C:/Users/victo/Documents/tiny-bdd-cec/qdd-sim/build/release/qdd-sim.exe";
        if (std::filesystem::exists(qdd_sim_bin)) {
            std::string cmd = "powershell -Command \"& '" + qdd_sim_bin + "' --ec '" + in_path + "' '" + out_path + "'\"";
            int ret = std::system(cmd.c_str());
            REQUIRE(ret == 0);
        }

        std::filesystem::remove(in_path);
        std::filesystem::remove(out_path);
    }

    SECTION("Double Toffoli Gate Verification via qdd-sim") {
        std::string qasm =
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[3];\n"
            "ccx q[0],q[1],q[2];\n"
            "ccx q[0],q[1],q[2];\n";

        auto ir = QasmParser::from_string(qasm);
        auto g = CircuitToGraph::convert(ir);
        Simplifier::full_reduce(g);
        auto ir_extracted = CircuitExtractor::extract(g);
        auto ir_opt = PostOptimizer::optimize(ir_extracted);

        std::string in_path = "temp_e2e_tof2_in.qasm";
        std::string out_path = "temp_e2e_tof2_out.qasm";
        QasmWriter::to_file(ir, in_path);
        QasmWriter::to_file(ir_opt, out_path);

        std::string qdd_sim_bin = "C:/Users/victo/Documents/tiny-bdd-cec/qdd-sim/build/release/qdd-sim.exe";
        if (std::filesystem::exists(qdd_sim_bin)) {
            std::string cmd = "powershell -Command \"& '" + qdd_sim_bin + "' --ec '" + in_path + "' '" + out_path + "'\"";
            int ret = std::system(cmd.c_str());
            REQUIRE(ret == 0);
        }

        std::filesystem::remove(in_path);
        std::filesystem::remove(out_path);
    }
}
