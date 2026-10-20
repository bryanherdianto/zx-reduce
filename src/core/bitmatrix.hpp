#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <string>
#include <bit>

namespace zx {

struct RowOp {
    size_t ctrl = 0; // source row
    size_t tgt = 0;  // target row: tgt ^= ctrl
};

/**
 * @brief Matrix over F_2 (GF(2)) with row-major 64-bit bit-packed storage.
 */
class BitMatrix {
public:
    size_t rows = 0;
    size_t cols = 0;
    size_t words_per_row = 0;
    std::vector<uint64_t> data; // rows * words_per_row

    BitMatrix() = default;
    BitMatrix(size_t r, size_t c);

    static BitMatrix identity(size_t n);
    static BitMatrix zeros(size_t r, size_t c);

    [[nodiscard]] bool get(size_t r, size_t c) const noexcept {
        size_t word_idx = r * words_per_row + (c / 64);
        size_t bit_idx = c % 64;
        return (data[word_idx] >> bit_idx) & 1ULL;
    }

    void set(size_t r, size_t c, bool val) noexcept {
        size_t word_idx = r * words_per_row + (c / 64);
        size_t bit_idx = c % 64;
        if (val) {
            data[word_idx] |= (1ULL << bit_idx);
        } else {
            data[word_idx] &= ~(1ULL << bit_idx);
        }
    }

    void row_xor(size_t target_row, size_t source_row) noexcept {
        size_t tgt_offset = target_row * words_per_row;
        size_t src_offset = source_row * words_per_row;
        for (size_t w = 0; w < words_per_row; ++w) {
            data[tgt_offset + w] ^= data[src_offset + w];
        }
    }

    void row_swap(size_t r1, size_t r2) noexcept {
        if (r1 == r2) return;
        size_t o1 = r1 * words_per_row;
        size_t o2 = r2 * words_per_row;
        for (size_t w = 0; w < words_per_row; ++w) {
            std::swap(data[o1 + w], data[o2 + w]);
        }
    }

    [[nodiscard]] size_t row_popcount(size_t r) const noexcept {
        size_t cnt = 0;
        size_t offset = r * words_per_row;
        for (size_t w = 0; w < words_per_row; ++w) {
            cnt += static_cast<size_t>(std::popcount(data[offset + w]));
        }
        return cnt;
    }

    [[nodiscard]] std::optional<size_t> find_pivot_in_col(size_t start_row, size_t col) const noexcept {
        for (size_t r = start_row; r < rows; ++r) {
            if (get(r, col)) {
                return r;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] BitMatrix transpose() const;
    [[nodiscard]] BitMatrix matmul(const BitMatrix& other) const;

    /**
     * @brief Performs Gauss-Jordan elimination to Reduced Row Echelon Form (RREF).
     * @param record_ops If true, records elementary row additions into ops.
     * @return Pair of (rank, list of row XOR operations).
     */
    std::pair<size_t, std::vector<RowOp>> gauss_jordan(bool record_ops = true);

    /**
     * @brief Inverts a square matrix over F_2.
     * @return Inverted matrix and list of CNOT row ops, or std::nullopt if singular.
     */
    [[nodiscard]] std::optional<std::pair<BitMatrix, std::vector<RowOp>>> inverse() const;

    [[nodiscard]] bool operator==(const BitMatrix& other) const noexcept;
    [[nodiscard]] bool operator!=(const BitMatrix& other) const noexcept { return !(*this == other); }

    [[nodiscard]] std::string to_string() const;
};

} // namespace zx
