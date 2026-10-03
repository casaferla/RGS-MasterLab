#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <vector>

namespace rgsml::tests::oracle {

struct ThdResult final {
    double non_fundamental_ratio_db{-std::numeric_limits<double>::infinity()};
    bool non_fundamental_is_minus_inf{true};
    double thd_ratio_db{-std::numeric_limits<double>::infinity()};
    bool thd_is_minus_inf{true};
};

[[nodiscard]] inline ThdResult analyze_thd_and_non_fundamental(
    const std::vector<double>& signal,
    double sample_rate,
    double f0_hz)
{
    const std::size_t N = signal.size();
    if (N == 0) return {};

    const double pi = std::numbers::pi_v<double>;

    // Exact 3x3 synchronous least-squares fit for DC, cos(w0*n), sin(w0*n)
    // Basis functions: f0 = 1, f1 = cos(w0*n), f2 = sin(w0*n)
    double M[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
    double B[3] = {0.0, 0.0, 0.0};

    M[0][0] = static_cast<double>(N);

    for (std::size_t n = 0; n < N; ++n) {
        const double phase = 2.0 * pi * f0_hz * static_cast<double>(n) / sample_rate;
        const double c = std::cos(phase);
        const double s = std::sin(phase);
        const double y = signal[n];

        M[0][1] += c;
        M[0][2] += s;
        M[1][1] += c * c;
        M[1][2] += c * s;
        M[2][2] += s * s;

        B[0] += y;
        B[1] += y * c;
        B[2] += y * s;
    }

    M[1][0] = M[0][1];
    M[2][0] = M[0][2];
    M[2][1] = M[1][2];

    // Solve 3x3 system M * X = B via Gaussian elimination with pivoting
    double X[3] = {0.0, 0.0, 0.0};
    double A[3][4] = {
        {M[0][0], M[0][1], M[0][2], B[0]},
        {M[1][0], M[1][1], M[1][2], B[1]},
        {M[2][0], M[2][1], M[2][2], B[2]}
    };

    for (std::size_t i = 0; i < 3; ++i) {
        std::size_t pivot = i;
        for (std::size_t j = i + 1; j < 3; ++j) {
            if (std::abs(A[j][i]) > std::abs(A[pivot][i])) pivot = j;
        }
        for (std::size_t k = 0; k <= 3; ++k) std::swap(A[i][k], A[pivot][k]);

        if (std::abs(A[i][i]) > 1e-15) {
            const double factor = A[i][i];
            for (std::size_t k = i; k <= 3; ++k) A[i][k] /= factor;
            for (std::size_t j = 0; j < 3; ++j) {
                if (j != i) {
                    const double mult = A[j][i];
                    for (std::size_t k = i; k <= 3; ++k) A[j][k] -= mult * A[i][k];
                }
            }
        }
    }

    X[0] = A[0][3]; // DC
    X[1] = A[1][3]; // A_c
    X[2] = A[2][3]; // A_s

    const double dc = X[0];
    const double A_c = X[1];
    const double A_s = X[2];
    const double A1 = std::sqrt(A_c * A_c + A_s * A_s);

    // Reconstruct fundamental component
    double fund_energy = 0.0;
    double res_energy = 0.0;

    for (std::size_t n = 0; n < N; ++n) {
        const double phase = 2.0 * pi * f0_hz * static_cast<double>(n) / sample_rate;
        const double fund_sample = dc + A_c * std::cos(phase) + A_s * std::sin(phase);
        const double res_sample = signal[n] - fund_sample;

        fund_energy += (fund_sample - dc) * (fund_sample - dc);
        res_energy += res_sample * res_sample;
    }

    ThdResult res;
    if (res_energy <= 1e-30 || fund_energy <= 1e-30) {
        res.non_fundamental_ratio_db = -std::numeric_limits<double>::infinity();
        res.non_fundamental_is_minus_inf = true;
    } else {
        const double rms_res = std::sqrt(res_energy / N);
        const double rms_fund = std::sqrt(fund_energy / N);
        res.non_fundamental_ratio_db = 20.0 * std::log10(rms_res / rms_fund);
        res.non_fundamental_is_minus_inf = false;
    }

    // 2. Harmonic THD up to harmonic 10 or highest harmonic strictly below Nyquist (k * f0 < 0.5 * Fs)
    double harmonic_energy_sum = 0.0;
    std::size_t max_k = 10;
    while (max_k >= 2 && (static_cast<double>(max_k) * f0_hz >= 0.5 * sample_rate)) {
        --max_k;
    }

    for (std::size_t k = 2; k <= max_k; ++k) {
        double hk_c = 0.0;
        double hk_s = 0.0;
        const double fk = static_cast<double>(k) * f0_hz;
        for (std::size_t n = 0; n < N; ++n) {
            const double phase = 2.0 * pi * fk * static_cast<double>(n) / sample_rate;
            hk_c += signal[n] * std::cos(phase);
            hk_s += signal[n] * std::sin(phase);
        }
        hk_c *= (2.0 / N);
        hk_s *= (2.0 / N);
        const double Ak = std::sqrt(hk_c * hk_c + hk_s * hk_s);
        harmonic_energy_sum += Ak * Ak;
    }

    if (harmonic_energy_sum <= 1e-30 || A1 <= 1e-30) {
        res.thd_ratio_db = -std::numeric_limits<double>::infinity();
        res.thd_is_minus_inf = true;
    } else {
        const double thd_lin = std::sqrt(harmonic_energy_sum) / A1;
        res.thd_ratio_db = 20.0 * std::log10(thd_lin);
        res.thd_is_minus_inf = false;
    }

    return res;
}

}  // namespace rgsml::tests::oracle
