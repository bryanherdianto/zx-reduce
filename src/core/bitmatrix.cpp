#include "core/bitmatrix.hpp"
#include <sstream>
#include <stdexcept>

namespace zx {

BitMatrix::BitMatrix(size_t r, size_t c)
    : rows(r), cols(c), words_per_row((c + 63) / 64), data(r * ((c + 63) / 64), 0ULL) {}

BitMatrix BitMatrix::identity(size_t n) {
    BitMatrix m(n, n);
    for (size_t i = 0; i < n; ++i) {
        m.set(i, i, true);
    }
    return m;
}

BitMatrix BitMatrix::zeros(size_t r, size_t c) {
    return BitMatrix(r, c);
}

BitMatrix BitMatrix::transpose() const {
    BitMatrix res(cols, rows);
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            if (get(r, c)) {
                res.set(c, r, true);
            }
        }
    }
    return res;
}

BitMatrix BitMatrix::matmul(const BitMatrix& other) const {
    if (cols != other.rows) {
        throw std::invalid_argument("Matrix dimensions do not match for multiplication");
    }
    BitMatrix res(rows, other.cols);
    for (size_t i = 0; i < rows; ++i) {
        for (size_t k = 0; k < cols; ++k) {
            if (get(i, k)) {
                res.row_xor(i, k); // if other has k-th row, but need to be careful with other's rows
            }
        }
    }
    // Correct general multiplication:
    BitMatrix correct_res(rows, other.cols);
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < other.cols; ++j) {
            bool sum = false;
            for (size_t k = 0; k < cols; ++k) {
                if (get(i, k) && other.get(k, j)) {
                    sum = !sum;
                }
            }
            if (sum) {
                correct_res.set(i, j, true);
            }
        }
    }
    return correct_res;
}

std::pair<size_t, std::vector<RowOp>> BitMatrix::gauss_jordan(bool record_ops) {
    std::vector<RowOp> ops;
    size_t pivot_row = 0;

    for (size_t col = 0; col < cols && pivot_row < rows; ++col) {
        auto p = find_pivot_in_col(pivot_row, col);
        if (!p.has_value()) {
            continue;
        }

        size_t pr = p.value();
        if (pr != pivot_row) {
            // Swap rows via 3 XORs if we want to record ops, or just swap data:
            if (record_ops) {
                // Row swap in F2: A ^= B; B ^= A; A ^= B
                row_xor(pivot_row, pr);
                ops.push_back({pr, pivot_row});
                row_xor(pr, pivot_row);
                ops.push_back({pivot_row, pr});
                row_xor(pivot_row, pr);
                ops.push_back({pr, pivot_row});
            } else {
                row_swap(pivot_row, pr);
            }
        }

        // Eliminate all other rows in this column
        for (size_t r = 0; r < rows; ++r) {
            if (r != pivot_row && get(r, col)) {
                row_xor(r, pivot_row);
                if (record_ops) {
                    ops.push_back({pivot_row, r});
                }
            }
        }

        pivot_row++;
    }

    return {pivot_row, ops};
}

std::optional<std::pair<BitMatrix, std::vector<RowOp>>> BitMatrix::inverse() const {
    if (rows != cols) {
        return std::nullopt;
    }
    size_t n = rows;
    // Augmented matrix [A | I]
    BitMatrix aug(n, 2 * n);
    for (size_t r = 0; r < n; ++r) {
        for (size_t c = 0; c < n; ++c) {
            aug.set(r, c, get(r, c));
        }
        aug.set(r, n + r, true);
    }

    std::vector<RowOp> ops;
    size_t pivot_row = 0;

    for (size_t col = 0; col < n; ++col) {
        auto p = aug.find_pivot_in_col(pivot_row, col);
        if (!p.has_value()) {
            continue;
        }

        size_t pr = p.value();
        if (pr != pivot_row) {
            aug.row_xor(pivot_row, pr);
            ops.push_back({pr, pivot_row});
            aug.row_xor(pr, pivot_row);
            ops.push_back({pivot_row, pr});
            aug.row_xor(pivot_row, pr);
            ops.push_back({pr, pivot_row});
        }

        for (size_t r = 0; r < n; ++r) {
            if (r != pivot_row && aug.get(r, col)) {
                aug.row_xor(r, pivot_row);
                ops.push_back({pivot_row, r});
            }
        }

        pivot_row++;
    }

    if (pivot_row != n) {
        return std::nullopt; // Singular: rank of left block is < n
    }

    // Extract right half
    BitMatrix inv(n, n);
    for (size_t r = 0; r < n; ++r) {
        for (size_t c = 0; c < n; ++c) {
            inv.set(r, c, aug.get(r, n + c));
        }
    }

    return std::make_pair(inv, ops);
}

bool BitMatrix::operator==(const BitMatrix& other) const noexcept {
    if (rows != other.rows || cols != other.cols) return false;
    return data == other.data;
}

std::string BitMatrix::to_string() const {
    std::ostringstream oss;
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            oss << (get(r, c) ? '1' : '.');
        }
        oss << '\n';
    }
    return oss.str();
}

} // namespace zx
