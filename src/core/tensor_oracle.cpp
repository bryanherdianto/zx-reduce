#include "core/tensor_oracle.hpp"
#include <cmath>
#include <numbers>

namespace zx {

ComplexMatrix TensorOracle::to_matrix(const Graph& g) {
    size_t n_in = g.inputs.size();
    size_t n_out = g.outputs.size();
    size_t dim_in = 1ULL << n_in;
    size_t dim_out = 1ULL << n_out;

    ComplexMatrix mat(dim_out, std::vector<std::complex<double>>(dim_in, 0.0));

    // Collect interior vertices
    std::vector<VertexId> interior;
    for (size_t i = 0; i < g.vertices.size(); ++i) {
        if (!g.vertices[i].active) continue;
        if (g.vertices[i].type != VertexType::Boundary) {
            interior.push_back(static_cast<VertexId>(i));
        }
    }

    size_t n_interior = interior.size();
    if (n_interior > 16) {
        throw std::invalid_argument("TensorOracle only supports diagrams with <= 16 interior spiders");
    }

    size_t total_assignments = 1ULL << n_interior;

    // Build edge list for fast contraction
    struct EdgeInfo {
        size_t u_idx; // index in full vertex vector
        size_t v_idx;
        EdgeCount count;
    };

    std::vector<EdgeInfo> edges;
    for (size_t i = 0; i < g.vertices.size(); ++i) {
        if (!g.vertices[i].active) continue;
        for (const auto& [nbr, count] : g.vertices[i].neighbors) {
            if (i < nbr) {
                edges.push_back({i, nbr, count});
            }
        }
    }

    // Map vertex id to its assignment index
    std::unordered_map<VertexId, size_t> int_idx;
    for (size_t k = 0; k < n_interior; ++k) {
        int_idx[interior[k]] = k;
    }

    const double inv_sqrt2 = 1.0 / std::sqrt(2.0);

    for (size_t in_state = 0; in_state < dim_in; ++in_state) {
        for (size_t out_state = 0; out_state < dim_out; ++out_state) {
            std::complex<double> element_sum = 0.0;

            // Iterate over all internal spin configurations
            for (size_t mask = 0; mask < total_assignments; ++mask) {
                // Determine bit for every vertex
                auto get_spin = [&](VertexId v) -> int {
                    if (g.vertices[v].type == VertexType::Boundary) {
                        // Check if input
                        for (size_t k = 0; k < n_in; ++k) {
                            if (g.inputs[k] == v) {
                                return (in_state >> k) & 1;
                            }
                        }
                        for (size_t k = 0; k < n_out; ++k) {
                            if (g.outputs[k] == v) {
                                return (out_state >> k) & 1;
                            }
                        }
                    }
                    size_t k = int_idx[v];
                    return (mask >> k) & 1;
                };

                std::complex<double> term = 1.0;

                // 1. Vertex phase contributions
                for (VertexId v : interior) {
                    int spin = (mask >> int_idx[v]) & 1;
                    if (spin == 1) {
                        term *= std::polar(1.0, g.vertices[v].phase.to_double());
                    }
                }

                // 2. Edge contractions
                for (const auto& e : edges) {
                    int s_u = get_spin(static_cast<VertexId>(e.u_idx));
                    int s_v = get_spin(static_cast<VertexId>(e.v_idx));

                    // Simple edges
                    for (uint8_t k = 0; k < e.count.n_simple; ++k) {
                        if (s_u != s_v) {
                            term = 0.0;
                            break;
                        }
                    }
                    if (term == 0.0) break;

                    // Hadamard edges: H(0,0)=1, H(0,1)=1, H(1,0)=1, H(1,1)=-1
                    for (uint8_t k = 0; k < e.count.n_hadamard; ++k) {
                        double h_val = (s_u == 1 && s_v == 1) ? -inv_sqrt2 : inv_sqrt2;
                        term *= h_val;
                    }
                }

                element_sum += term;
            }

            // Apply diagram scalar
            element_sum *= g.scalar.to_complex();
            mat[out_state][in_state] = element_sum;
        }
    }

    return mat;
}

bool TensorOracle::are_equal(const ComplexMatrix& A, const ComplexMatrix& B, double tol) {
    if (A.size() != B.size()) return false;
    for (size_t r = 0; r < A.size(); ++r) {
        if (A[r].size() != B[r].size()) return false;
        for (size_t c = 0; c < A[r].size(); ++c) {
            if (std::abs(A[r][c] - B[r][c]) > tol) {
                return false;
            }
        }
    }
    return true;
}

bool TensorOracle::are_equal_up_to_global_phase(const ComplexMatrix& A, const ComplexMatrix& B, double tol) {
    if (A.size() != B.size()) return false;
    std::optional<std::complex<double>> phase_ratio;

    for (size_t r = 0; r < A.size(); ++r) {
        if (A[r].size() != B[r].size()) return false;
        for (size_t c = 0; c < A[r].size(); ++c) {
            double mag_a = std::abs(A[r][c]);
            double mag_b = std::abs(B[r][c]);

            if (mag_a > tol && mag_b > tol) {
                std::complex<double> ratio = A[r][c] / B[r][c];
                if (!phase_ratio.has_value()) {
                    phase_ratio = ratio / std::abs(ratio);
                } else {
                    std::complex<double> cur = ratio / std::abs(ratio);
                    if (std::abs(cur - phase_ratio.value()) > tol) {
                        return false;
                    }
                }
                if (std::abs(mag_a - mag_b) > tol) {
                    return false;
                }
            } else if (mag_a > tol || mag_b > tol) {
                return false;
            }
        }
    }
    return true;
}

} // namespace zx
