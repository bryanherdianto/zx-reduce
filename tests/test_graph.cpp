#include <catch2/catch_test_macros.hpp>
#include "core/graph.hpp"

using namespace zx;

TEST_CASE("Graph Creation and Invariants", "[graph]") {
    Graph g;

    auto in0 = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 0);
    auto out0 = g.add_vertex(VertexType::Boundary, Phase::zero(), 0, 1);
    g.inputs.push_back(in0);
    g.outputs.push_back(out0);

    auto z0 = g.add_vertex(VertexType::Z, Phase::pi_over_4(), 0, 0);

    g.add_edge(in0, z0, EdgeType::Simple);
    g.add_edge(z0, out0, EdgeType::Simple);

    REQUIRE(g.is_valid_vertex(in0));
    REQUIRE(g.is_valid_vertex(z0));
    REQUIRE(g.degree(in0) == 1);
    REQUIRE(g.degree(out0) == 1);
    REQUIRE(g.degree(z0) == 2);
    REQUIRE(g.validate());

    SECTION("Hopf multigraph edge cancellation") {
        auto z1 = g.add_vertex(VertexType::Z, Phase::zero(), 1, 0);
        g.add_edge(z0, z1, EdgeType::Hadamard);
        REQUIRE(g.edge_count(z0, z1).n_hadamard == 1);

        // Add second Hadamard edge between Z spiders -> cancels to 0
        g.add_edge(z0, z1, EdgeType::Hadamard);
        REQUIRE(g.edge_count(z0, z1).n_hadamard == 0);
        REQUIRE(!g.has_edge(z0, z1));
    }

    SECTION("Hadamard self-loop adds pi to phase") {
        Phase init_phase = g.vertices[z0].phase;
        g.add_edge(z0, z0, EdgeType::Hadamard); // Self loop
        REQUIRE(g.vertices[z0].phase == init_phase + Phase::pi());
        REQUIRE(g.vertices[z0].self_hadamard == 0);
    }
}
