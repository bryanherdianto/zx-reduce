#pragma once

#include "core/phase.hpp"
#include <complex>
#include <cmath>

namespace zx {

/**
 * @brief Tracks the global scalar factor of a ZX diagram: c = exp(i * phase) * 2^(power_of_sqrt2 / 2).
 */
class Scalar {
public:
    int32_t power_of_sqrt2 = 0; // Exponent p in 2^(p/2)
    Phase phase;                // Global phase angle

    constexpr Scalar() noexcept : power_of_sqrt2(0), phase(Phase::zero()) {}
    Scalar(int32_t p, Phase ph) noexcept : power_of_sqrt2(p), phase(ph) {}

    void mul_sqrt2(int32_t power = 1) noexcept {
        power_of_sqrt2 += power;
    }

    void div_sqrt2(int32_t power = 1) noexcept {
        power_of_sqrt2 -= power;
    }

    void mul_two(int32_t count = 1) noexcept {
        power_of_sqrt2 += 2 * count;
    }

    void div_two(int32_t count = 1) noexcept {
        power_of_sqrt2 -= 2 * count;
    }

    void mul_phase(Phase ph) noexcept {
        phase += ph;
    }

    [[nodiscard]] std::complex<double> to_complex() const noexcept {
        double mag = std::pow(std::sqrt(2.0), power_of_sqrt2);
        double angle = phase.to_double();
        return std::polar(mag, angle);
    }

    [[nodiscard]] bool is_one() const noexcept {
        return power_of_sqrt2 == 0 && phase.is_zero();
    }
};

} // namespace zx
