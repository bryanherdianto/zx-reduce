#pragma once

#include "core/graph.hpp"
#include <vector>
#include <complex>

namespace zx {

using ComplexMatrix = std::vector<std::vector<std::complex<double>>>;

/**
 * @brief Test-only exact tensor contraction engine for small ZX diagrams (<= 8 spiders).
 * Evaluates the diagram into an explicit 2^(n_out) x 2^(n_in) dense linear map.
 */
class TensorOracle {
public:
    static ComplexMatrix to_matrix(const Graph& g);
    static bool are_equal(const ComplexMatrix& A, const ComplexMatrix& B, double tol = 1e-9);
    static bool are_equal_up_to_global_phase(const ComplexMatrix& A, const ComplexMatrix& B, double tol = 1e-9);
};

} // namespace zx
