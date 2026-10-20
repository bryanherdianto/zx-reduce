#pragma once

#include <cstdint>
#include <numeric>
#include <string>
#include <stdexcept>
#include <numbers>

namespace zx {

/**
 * @brief Represents a rational angle alpha = (num / den) * pi in [0, 2*pi).
 * Normalized such that den > 0, gcd(num, den) == 1, and 0 <= num < 2 * den.
 */
class Phase {
public:
    int32_t num = 0;
    int32_t den = 1;

    constexpr Phase() noexcept : num(0), den(1) {}

    Phase(int64_t n, int64_t d) {
        if (d == 0) {
            throw std::invalid_argument("Phase denominator cannot be zero");
        }
        if (d < 0) {
            n = -n;
            d = -d;
        }
        int64_t g = std::gcd(std::abs(n), d);
        n /= g;
        d /= g;
        int64_t mod = 2 * d;
        n = (n % mod + mod) % mod;
        
        num = static_cast<int32_t>(n);
        den = static_cast<int32_t>(d);
    }

    constexpr static Phase zero() noexcept { return Phase(); }
    static Phase pi() noexcept { return Phase(1, 1); }
    static Phase half_pi() noexcept { return Phase(1, 2); }
    static Phase minus_half_pi() noexcept { return Phase(3, 2); }
    static Phase pi_over_4() noexcept { return Phase(1, 4); }
    static Phase minus_pi_over_4() noexcept { return Phase(7, 4); }

    [[nodiscard]] constexpr bool is_zero() const noexcept {
        return num == 0;
    }

    [[nodiscard]] constexpr bool is_pauli() const noexcept {
        return num == 1 && den == 1; // pi
    }

    [[nodiscard]] constexpr bool is_proper_clifford() const noexcept {
        return den == 2; // pi/2 or 3*pi/2
    }

    [[nodiscard]] constexpr bool is_clifford() const noexcept {
        return is_zero() || is_pauli() || is_proper_clifford();
    }

    [[nodiscard]] constexpr bool is_t_like() const noexcept {
        return den == 4 && (num % 2 != 0); // 1/4, 3/4, 5/4, 7/4
    }

    [[nodiscard]] Phase operator-() const noexcept {
        if (num == 0) return *this;
        return Phase(2 * static_cast<int64_t>(den) - num, den);
    }

    Phase& operator+=(const Phase& other) {
        int64_t n = static_cast<int64_t>(num) * other.den + static_cast<int64_t>(other.num) * den;
        int64_t d = static_cast<int64_t>(den) * other.den;
        *this = Phase(n, d);
        return *this;
    }

    Phase& operator-=(const Phase& other) {
        int64_t n = static_cast<int64_t>(num) * other.den - static_cast<int64_t>(other.num) * den;
        int64_t d = static_cast<int64_t>(den) * other.den;
        *this = Phase(n, d);
        return *this;
    }

    [[nodiscard]] friend Phase operator+(Phase lhs, const Phase& rhs) {
        lhs += rhs;
        return lhs;
    }

    [[nodiscard]] friend Phase operator-(Phase lhs, const Phase& rhs) {
        lhs -= rhs;
        return lhs;
    }

    [[nodiscard]] constexpr bool operator==(const Phase& other) const noexcept {
        return num == other.num && den == other.den;
    }

    [[nodiscard]] constexpr bool operator!=(const Phase& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] constexpr bool operator<(const Phase& other) const noexcept {
        return static_cast<int64_t>(num) * other.den < static_cast<int64_t>(other.num) * den;
    }

    [[nodiscard]] double to_double() const noexcept {
        return (static_cast<double>(num) / den) * std::numbers::pi;
    }

    [[nodiscard]] std::string to_string() const;
};

} // namespace zx
