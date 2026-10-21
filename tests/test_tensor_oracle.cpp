#include <catch2/catch_test_macros.hpp>
#include "core/graph.hpp"
#include "core/tensor_oracle.hpp"

using namespace zx;

TEST_CASE("TensorOracle Exact Evaluation", "[tensor_oracle]") {
    SECTION("Identity wire: Boundary -> Z(0) -> Boundary") {
        Graph g;
        auto in = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 0);
        auto out = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 1);
        g.inputs.push_back(in);
        g.outputs.push_back(out);

        auto z = g.add_vertex(VertexType::Z, Phase::zero(), 0, 0);
        g.add_edge(in, z, EdgeType::Simple);
        g.add_edge(z, out, EdgeType::Simple);

        auto mat = TensorOracle::to_matrix(g);
        // Expect 2x2 identity matrix
        REQUIRE(std::abs(mat[0][0] - 1.0) < 1e-9);
        REQUIRE(std::abs(mat[0][1]) < 1e-9);
        REQUIRE(std::abs(mat[1][0]) < 1e-9);
        REQUIRE(std::abs(mat[1][1] - 1.0) < 1e-9);
    }

    SECTION("T-gate: Boundary -> Z(pi/4) -> Boundary") {
        Graph g;
        auto in = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 0);
        auto out = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 1);
        g.inputs.push_back(in);
        g.outputs.push_back(out);

        auto z = g.add_vertex(VertexType::Z, Phase::pi_over_4(), 0, 0);
        g.add_edge(in, z, EdgeType::Simple);
        g.add_edge(z, out, EdgeType::Simple);

        auto mat = TensorOracle::to_matrix(g);
        REQUIRE(std::abs(mat[0][0] - 1.0) < 1e-9);
        REQUIRE(std::abs(mat[0][1]) < 1e-9);
        REQUIRE(std::abs(mat[1][0]) < 1e-9);
        // exp(i * pi/4) = 1/sqrt(2) + i/sqrt(2)
        double expected = 1.0 / std::sqrt(2.0);
        REQUIRE(std::abs(mat[1][1].real() - expected) < 1e-9);
        REQUIRE(std::abs(mat[1][1].imag() - expected) < 1e-9);
    }
}
