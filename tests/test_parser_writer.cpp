#include <catch2/catch_test_macros.hpp>
#include "io/qasm_parser.hpp"
#include "io/qasm_writer.hpp"
#include "zx/from_circuit.hpp"
#include "zx/to_graph_like.hpp"

using namespace zx;

TEST_CASE("QASM Parser and Writer", "[parser_writer]") {
    std::string qasm =
        "OPENQASM 2.0;\n"
        "include \"qelib1.inc\";\n"
        "qreg q[2];\n"
        "h q[0];\n"
        "cx q[0],q[1];\n"
        "t q[1];\n";

    auto ir = QasmParser::from_string(qasm);
    REQUIRE(ir.num_qubits == 2);
    REQUIRE(ir.gates.size() == 3);
    REQUIRE(ir.t_count() == 1);
    REQUIRE(ir.cnot_count() == 1);

    std::string emitted = QasmWriter::to_string(ir);
    REQUIRE(emitted.find("h q[0];") != std::string::npos);
    REQUIRE(emitted.find("cx q[0],q[1];") != std::string::npos);
    REQUIRE(emitted.find("t q[1];") != std::string::npos);

    SECTION("Convert to Graph and Graph-Like") {
        auto g = CircuitToGraph::convert(ir);
        REQUIRE(g.inputs.size() == 2);
        REQUIRE(g.outputs.size() == 2);
        REQUIRE(g.validate());

        ToGraphLike::convert(g);
        REQUIRE(g.validate());
        REQUIRE(g.is_graph_like());
    }
}
