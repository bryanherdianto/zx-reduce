#include <catch2/catch_test_macros.hpp>
#include "core/phase.hpp"
#include "core/scalar.hpp"

using namespace zx;

TEST_CASE("Phase Normalization and Arithmetic", "[phase]") {
    SECTION("Default constructor is zero") {
        Phase p;
        REQUIRE(p.is_zero());
        REQUIRE(p.num == 0);
        REQUIRE(p.den == 1);
        REQUIRE(p.to_string() == "0");
    }

    SECTION("Normalization to [0, 2*pi)") {
        Phase p1(2, 1); // 2*pi == 0
        REQUIRE(p1.is_zero());

        Phase p2(5, 2); // 5/2 pi == 1/2 pi
        REQUIRE(p2.num == 1);
        REQUIRE(p2.den == 2);
        REQUIRE(p2.to_string() == "pi/2");

        Phase p3(-1, 2); // -1/2 pi == 3/2 pi
        REQUIRE(p3.num == 3);
        REQUIRE(p3.den == 2);
        REQUIRE(p3.to_string() == "3*pi/2");

        Phase p4(-7, 4); // -7/4 pi == 1/4 pi
        REQUIRE(p4.num == 1);
        REQUIRE(p4.den == 4);
        REQUIRE(p4.to_string() == "pi/4");
    }

    SECTION("Predicates") {
        REQUIRE(Phase::zero().is_zero());
        REQUIRE(Phase::pi().is_pauli());
        REQUIRE(Phase::half_pi().is_proper_clifford());
        REQUIRE(Phase::minus_half_pi().is_proper_clifford());
        REQUIRE(Phase::pi_over_4().is_t_like());
        REQUIRE(Phase::minus_pi_over_4().is_t_like());

        REQUIRE_FALSE(Phase::zero().is_proper_clifford());
        REQUIRE_FALSE(Phase::pi().is_proper_clifford());
        REQUIRE_FALSE(Phase::half_pi().is_t_like());
    }

    SECTION("Addition and Subtraction") {
        Phase p1 = Phase::pi_over_4();
        Phase p2 = Phase::pi_over_4();
        Phase sum = p1 + p2;
        REQUIRE(sum == Phase::half_pi());

        Phase p3 = Phase::half_pi();
        Phase p4 = Phase::half_pi();
        REQUIRE(p3 + p4 == Phase::pi());
        REQUIRE(p3 + p4 + p3 + p4 == Phase::zero());

        REQUIRE(p1 - p2 == Phase::zero());
        REQUIRE(-Phase::half_pi() == Phase::minus_half_pi());
    }

    SECTION("Zero denominator throws") {
        REQUIRE_THROWS_AS(Phase(1, 0), std::invalid_argument);
    }
}

TEST_CASE("Scalar Arithmetic", "[scalar]") {
    Scalar s;
    REQUIRE(s.is_one());

    s.mul_sqrt2(2);
    REQUIRE(s.power_of_sqrt2 == 2);
    // 2^(2/2) = 2
    auto c = s.to_complex();
    REQUIRE(std::abs(c.real() - 2.0) < 1e-12);
    REQUIRE(std::abs(c.imag()) < 1e-12);

    s.mul_phase(Phase::half_pi());
    c = s.to_complex();
    // 2 * exp(i * pi/2) = 2i
    REQUIRE(std::abs(c.real()) < 1e-12);
    REQUIRE(std::abs(c.imag() - 2.0) < 1e-12);
}
