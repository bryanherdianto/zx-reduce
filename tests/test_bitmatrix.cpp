#include <catch2/catch_test_macros.hpp>
#include "core/bitmatrix.hpp"

using namespace zx;

TEST_CASE("BitMatrix Basic Operations", "[bitmatrix]") {
    SECTION("Identity and indexing") {
        auto I = BitMatrix::identity(4);
        REQUIRE(I.rows == 4);
        REQUIRE(I.cols == 4);
        for (size_t r = 0; r < 4; ++r) {
            for (size_t c = 0; c < 4; ++c) {
                REQUIRE(I.get(r, c) == (r == c));
            }
        }
    }

    SECTION("Row XOR and Transpose") {
        BitMatrix m(2, 2);
        m.set(0, 0, true);
        m.set(1, 1, true);
        m.row_xor(1, 0); // row 1 = [1, 1]
        REQUIRE(m.get(1, 0) == true);
        REQUIRE(m.get(1, 1) == true);

        auto mt = m.transpose();
        REQUIRE(mt.get(0, 1) == true);
        REQUIRE(mt.get(1, 1) == true);
        REQUIRE(mt.get(1, 0) == false);
    }

    SECTION("Matrix multiplication over F2") {
        auto I = BitMatrix::identity(3);
        BitMatrix A(3, 3);
        A.set(0, 1, true);
        A.set(1, 2, true);
        A.set(2, 0, true);

        auto res = A.matmul(I);
        REQUIRE(res == A);

        auto A2 = A.matmul(A);
        auto A3 = A2.matmul(A);
        REQUIRE(A3 == I); // Permutation of order 3
    }

    SECTION("Gauss-Jordan and Inversion") {
        // [1 1 0]
        // [0 1 1]
        // [1 0 1]
        // Over F2: det = 1 + 1 = 0? Let's check:
        // row0 + row1 = [1 0 1] == row2 -> singular!
        BitMatrix sing(3, 3);
        sing.set(0, 0, true); sing.set(0, 1, true);
        sing.set(1, 1, true); sing.set(1, 2, true);
        sing.set(2, 0, true); sing.set(2, 2, true);
        REQUIRE(sing.inverse() == std::nullopt);

        // Invertible matrix:
        // [1 1 0]
        // [0 1 1]
        // [0 0 1]
        BitMatrix inv_m(3, 3);
        inv_m.set(0, 0, true); inv_m.set(0, 1, true);
        inv_m.set(1, 1, true); inv_m.set(1, 2, true);
        inv_m.set(2, 2, true);

        auto inv_res = inv_m.inverse();
        REQUIRE(inv_res.has_value());
        auto [inv, ops] = inv_res.value();
        REQUIRE(inv_m.matmul(inv) == BitMatrix::identity(3));
    }
}
