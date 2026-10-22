#include <catch2/catch_test_macros.hpp>
#include "core/graph.hpp"
#include "core/tensor_oracle.hpp"
#include "zx/rules/local_comp.hpp"
#include "zx/rules/pivot.hpp"
#include <iostream>

using namespace zx;

TEST_CASE("Local Complementation Rule Soundness", "[rules]") {
    // Construct a small 2-qubit circuit with a pi/2 rotation and verify tensor before == tensor after
    Graph g;
    auto in0 = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 0);
    auto in1 = g.add_vertex(VertexType::Boundary, Phase::zero(), 1, 0);
    auto out0 = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 2);
    auto out1 = g.add_vertex(VertexType::Boundary, Phase::zero(), 1, 2);
    g.inputs = {in0, in1};
    g.outputs = {out0, out1};

    // Interior spider with pi/2 phase
    auto u = g.add_vertex(VertexType::Z, Phase::half_pi(), 0, 1);
    auto v1 = g.add_vertex(VertexType::Z, Phase::zero(), 0, 1);
    auto v2 = g.add_vertex(VertexType::Z, Phase::zero(), 1, 1);

    g.add_edge(in0, v1, EdgeType::Simple);
    g.add_edge(v1, out0, EdgeType::Simple);
    g.add_edge(in1, v2, EdgeType::Simple);
    g.add_edge(v2, out1, EdgeType::Simple);

    g.add_edge(u, v1, EdgeType::Hadamard);
    g.add_edge(u, v2, EdgeType::Hadamard);

    REQUIRE(g.validate());
    auto mat_before = TensorOracle::to_matrix(g);

    auto matches = LocalCompRule::find(g);
    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0].v == u);

    bool applied = LocalCompRule::apply(g, matches[0]);
    REQUIRE(applied);
    REQUIRE(g.validate());

    auto mat_after = TensorOracle::to_matrix(g);
    REQUIRE(TensorOracle::are_equal_up_to_global_phase(mat_before, mat_after));
}
