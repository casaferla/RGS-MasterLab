#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
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

    // 1. Least squares fit for DC, cos(w0*n), sin(w0*n)
    double sum_c2 = 0.0;
    double sum_s2 = 0.0;
    double sum_cs = 0.0;
    double sum_y = 0.0;
    double sum_yc = 0.0;
    double sum_ys = 0.0;

    for (std::size_t n = 0; n < N; ++n) {
        const double phase = 2.0 * M_PI * f0_hz * static_cast<double>(n) / sample_rate;
        const double c = std::cos(phase);
        const double s = std::sin(phase);
        const double y = signal[n];

        sum_c2 += c * c;
        sum_s2 += s * s;
        sum_cs += c * s;
        sum_y += y;
        sum_yc += y * c;
        sum_ys += y * s;
    }

    // Solve 2x2 system for fundamental A_c and A_s (dc = sum_y / N)
    const double dc = sum_y / static_cast<double>(N);

    // Subtract DC from sum_yc and sum_ys
    const double yc_ac = sum_yc - dc * (N * 0.0); // approx zero mean c
    const double ys_as = sum_ys - dc * (N * 0.0);

    const double det = sum_c2 * sum_s2 - sum_cs * sum_cs;
    double A_c = 0.0;
    double A_s = 0.0;
    if (std::abs(det) > 1e-12) {
        A_c = (sum_s2 * yc_ac - sum_cs * ys_as) / det;
        A_s = (sum_c2 * ys_as - sum_cs * yc_ac) / det;
    } else {
        A_c = (2.0 / N) * sum_yc;
        A_s = (2.0 / N) * sum_ys;
    }

    const double A1 = std::sqrt(A_c * A_c + A_s * A_s);

    // Reconstruct fundamental component
    double fund_energy = 0.0;
    double res_energy = 0.0;

    for (std::size_t n = 0; n < N; ++n) {
        const double phase = 2.0 * M_PI * f0_hz * static_cast<double>(n) / sample_rate;
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

    // 2. Harmonic THD up to harmonic 10 or Nyquist
    double harmonic_energy_sum = 0.0;
    const std::size_t max_k = std::min(std::size_t{10}, static_cast<std::size_t>(std::floor((sample_rate * 0.49) / f0_hz)));

    for (std::size_t k = 2; k <= max_k; ++k) {
        double hk_c = 0.0;
        double hk_s = 0.0;
        const double fk = static_cast<double>(k) * f0_hz;
        for (std::size_t n = 0; n < N; ++n) {
            const double phase = 2.0 * M_PI * fk * static_cast<double>(n) / sample_rate;
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
