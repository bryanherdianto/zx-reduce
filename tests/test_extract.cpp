#include <catch2/catch_test_macros.hpp>
#include "io/qasm_parser.hpp"
#include "io/qasm_writer.hpp"
#include "zx/from_circuit.hpp"
#include "zx/to_graph_like.hpp"
#include "zx/extract.hpp"
#include "core/tensor_oracle.hpp"

using namespace zx;

TEST_CASE("Circuit Extraction Round Trip", "[extract]") {
    SECTION("1-qubit circuit with H and T") {
        std::string qasm =
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[1];\n"
            "h q[0];\n"
            "t q[0];\n"
            "h q[0];\n";

        auto ir = QasmParser::from_string(qasm);
        auto g = CircuitToGraph::convert(ir);
        ToGraphLike::convert(g);
        auto mat_orig = TensorOracle::to_matrix(g);

        auto extracted_ir = CircuitExtractor::extract(g);
        auto g_ext = CircuitToGraph::convert(extracted_ir);
        ToGraphLike::convert(g_ext);
        auto mat_ext = TensorOracle::to_matrix(g_ext);

        REQUIRE(TensorOracle::are_equal_up_to_global_phase(mat_orig, mat_ext));
    }

    SECTION("2-qubit Bell circuit") {
        std::string qasm =
            "OPENQASM 2.0;\n"
            "include \"qelib1.inc\";\n"
            "qreg q[2];\n"
            "h q[0];\n"
            "cx q[0],q[1];\n";

        auto ir = QasmParser::from_string(qasm);
        auto g = CircuitToGraph::convert(ir);
        ToGraphLike::convert(g);
        auto mat_orig = TensorOracle::to_matrix(g);

        auto extracted_ir = CircuitExtractor::extract(g);
        auto g_ext = CircuitToGraph::convert(extracted_ir);
        ToGraphLike::convert(g_ext);
        auto mat_ext = TensorOracle::to_matrix(g_ext);

        REQUIRE(TensorOracle::are_equal_up_to_global_phase(mat_orig, mat_ext));
    }
}
