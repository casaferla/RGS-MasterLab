#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace rgsml::tests::oracle {

struct ModulationResult final {
    double effective_compression_ratio{1.0};
    double a_out_db_amplitude{0.0};
    double phase_lag_degrees{0.0};
};

[[nodiscard]] inline ModulationResult analyze_envelope_modulation(
    const std::vector<double>& l_in_delayed,
    const std::vector<double>& l_out,
    double sample_rate,
    double f_m_hz,
    std::size_t start_frame_index = 0)
{
    const std::size_t N = l_out.size();
    if (N == 0) return {};

    // Remove mean from l_in_delayed and l_out
    double mean_in = 0.0;
    double mean_out = 0.0;
    for (std::size_t n = 0; n < N; ++n) {
        mean_in += l_in_delayed[n];
        mean_out += l_out[n];
    }
    mean_in /= static_cast<double>(N);
    mean_out /= static_cast<double>(N);

    // Fit f_m on l_in_delayed and l_out
    double Sin = 0.0, Cin = 0.0;
    double Sout = 0.0, Cout = 0.0;

    for (std::size_t n = 0; n < N; ++n) {
        const double abs_n = static_cast<double>(start_frame_index + n);
        const double phase = 2.0 * M_PI * f_m_hz * abs_n / sample_rate;
        const double s = std::sin(phase);
        const double c = std::cos(phase);

        const double y_in = l_in_delayed[n] - mean_in;
        const double y_out = l_out[n] - mean_out;

        Sin += y_in * s;
        Cin += y_in * c;
        Sout += y_out * s;
        Cout += y_out * c;
    }

    Sin *= (2.0 / N);
    Cin *= (2.0 / N);
    Sout *= (2.0 / N);
    Cout *= (2.0 / N);

    const double Ain = std::sqrt(Sin * Sin + Cin * Cin);
    const double Aout = std::sqrt(Sout * Sout + Cout * Cout);

    const double phase_in = std::atan2(Cin, Sin);
    const double phase_out = std::atan2(Cout, Sout);

    double delta_rad = phase_out - phase_in;
    // Wrap to [-pi, pi]
    while (delta_rad > M_PI) delta_rad -= 2.0 * M_PI;
    while (delta_rad < -M_PI) delta_rad += 2.0 * M_PI;

    ModulationResult res;
    res.effective_compression_ratio = (Aout > 1e-12) ? (Ain / Aout) : 1.0;
    res.a_out_db_amplitude = Aout;
    res.phase_lag_degrees = std::abs(delta_rad * 180.0 / M_PI);

    return res;
}

}  // namespace rgsml::tests::oracle
