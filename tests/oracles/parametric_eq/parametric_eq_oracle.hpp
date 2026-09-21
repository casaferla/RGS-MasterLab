#pragma once

#include <rgsml/dsp/parametric_eq_parameters.hpp>

#include <complex>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace rgsml::tests::oracles {

struct IndependentBiquadCoeffs final {
    double b0{1.0};
    double b1{0.0};
    double b2{0.0};
    double a1{0.0};
    double a2{0.0};
};

// O1: PRODUCT OWNER AUTHORITATIVE FIXED HIGH-PRECISION REFERENCE CONSTANTS (100-decimal precision rounded to binary64)

namespace ref_constants {

// 48 kHz primitive fixtures
inline constexpr IndependentBiquadCoeffs BELL_1K_PLUS6_Q0707{
    1.0610510792184844, -1.8612559024730444, 0.81626552706657640,
    -1.8612559024730444, 0.87731660628506081};

inline constexpr IndependentBiquadCoeffs BELL_280_MINUS6_Q12{
    0.99892652951136351, -1.9943555763153522, 0.99676936858506038,
    -1.9943555763153522, 0.99569589809642389};

inline constexpr IndependentBiquadCoeffs BELL_1K_PLUS6_Q010{
    1.3145210666175664, -1.3562603596102321, 0.053442410172166208,
    -1.3562603596102321, 0.36796347678973257};

inline constexpr IndependentBiquadCoeffs NOTCH_1K_Q12{
    0.99459082693927603, -1.9721639290769479, 0.99459082693927603,
    -1.9721639290769479, 0.98918165387855195};

inline constexpr IndependentBiquadCoeffs LOW_SHELF_100_PLUS6_S05{
    1.0045903385248340, -1.9777108859045545, 0.97335990582378695,
    -1.9777705834283741, 0.97789054682480125};

inline constexpr IndependentBiquadCoeffs LOW_SHELF_100_PLUS6_S10{
    1.0032178957372331, -1.9843644307768977, 0.98138669874913176,
    -1.9844243291390486, 0.98454469612421402};

inline constexpr IndependentBiquadCoeffs LOW_SHELF_100_MINUS6_S05{
    0.99543063640092877, -1.9687334305171427, 0.97342220935626411,
    -1.9686740057730152, 0.96891227050132034};

inline constexpr IndependentBiquadCoeffs LOW_SHELF_100_MINUS6_S10{
    0.99679242590178452, -1.9780591410610335, 0.98138669605839046,
    -1.9779994348273175, 0.97823882819389074};

inline constexpr IndependentBiquadCoeffs HIGH_SHELF_10K_PLUS6_S05{
    1.4776010060227698, -0.63187986942224705, 0.057578220681078811,
    -0.091401498106326765, -0.0052991446120717210};

inline constexpr IndependentBiquadCoeffs HIGH_SHELF_10K_PLUS6_S10{
    1.4893047463718028, -0.74554363345151720, 0.32202546695273149,
    -0.10784297506329710, 0.17362955493631424};

inline constexpr IndependentBiquadCoeffs HIGH_SHELF_10K_MINUS6_S05{
    0.67677268486144360, -0.061858037273776915, -0.0035863163265808314,
    -0.42763903573879258, 0.038967366999878406};

inline constexpr IndependentBiquadCoeffs HIGH_SHELF_10K_MINUS6_S10{
    0.67145424899515593, -0.072411623830529473, 0.11658430241312605,
    -0.50059844049230828, 0.21622536807006074};

// Multi-sample-rate BELL fixture (1000 Hz, +3 dB, Q=1)
inline constexpr IndependentBiquadCoeffs BELL_1K_PLUS3_Q1_44100{
    1.0232544723373458, -1.8681385778428590, 0.86400683204351514,
    -1.8681385778428590, 0.88726130438086104};

inline constexpr IndependentBiquadCoeffs BELL_1K_PLUS3_Q1_48000{
    1.0214740964022990, -1.8796730201312601, 0.87441854812325048,
    -1.8796730201312601, 0.89589264452554951};

inline constexpr IndependentBiquadCoeffs BELL_1K_PLUS3_Q1_96000{
    1.0110469876194492, -1.9422762370302213, 0.93539673483227948,
    -1.9422762370302213, 0.94644372245172870};

// HP / LP Full Slope Matrix (1 kHz / 48 kHz)
inline constexpr IndependentBiquadCoeffs HP_6_SECTIONS[1]{
    {0.93848823149637839, -0.93848823149637839, 0.0, -0.87697646299275689, 0.0}
};

inline constexpr IndependentBiquadCoeffs HP_12_SECTIONS[1]{
    {0.91158666801283139, -1.8231733360256628, 0.91158666801283139, -1.8153410827045682, 0.83100558934675750}
};

inline constexpr IndependentBiquadCoeffs HP_18_SECTIONS[2]{
    {0.93848823149637839, -0.93848823149637839, 0.0, -0.87697646299275689, 0.0},
    {0.93471972728891184, -1.8694394545778237, 0.93471972728891184, -1.8614084445321082, 0.87747046462353917}
};

inline constexpr IndependentBiquadCoeffs HP_24_SECTIONS[2]{
    {0.88856942007384998, -1.7771388401477000, 0.88856942007384998, -1.7695043485128368, 0.78477333178256292},
    {0.94835204566440334, -1.8967040913288067, 0.94835204566440334, -1.8885559538890460, 0.90485222876856730}
};

inline constexpr IndependentBiquadCoeffs HP_36_SECTIONS[3]{
    {0.88423882039673307, -1.7684776407934661, 0.88423882039673307, -1.7608803571991478, 0.77607492438778447},
    {0.91158666801283139, -1.8231733360256628, 0.91158666801283139, -1.8153410827045682, 0.83100558934675750},
    {0.96318352488015890, -1.9263670497603178, 0.96318352488015890, -1.9180914818672381, 0.93464261765339729}
};

inline constexpr IndependentBiquadCoeffs HP_48_SECTIONS[4]{
    {0.88271843388844939, -1.7654368677768988, 0.88271843388844939, -1.7578526471777918, 0.77302108837600592},
    {0.89823795653467431, -1.7964759130693486, 0.89823795653467431, -1.7887583504227402, 0.80419347571595712},
    {0.92839826827756544, -1.8567965365551309, 0.92839826827756544, -1.8488198397964271, 0.86477323331383471},
    {0.97099658820377022, -1.9419931764075404, 0.97099658820377022, -1.9336504795257301, 0.95033587328935099}
};

inline constexpr IndependentBiquadCoeffs LP_6_SECTIONS[1]{
    {0.061511768503621569, 0.061511768503621569, 0.0, -0.87697646299275689, 0.0}
};

inline constexpr IndependentBiquadCoeffs LP_12_SECTIONS[1]{
    {0.0039161266605473692, 0.0078322533210947384, 0.0039161266605473692, -1.8153410827045682, 0.83100558934675750}
};

inline constexpr IndependentBiquadCoeffs LP_18_SECTIONS[2]{
    {0.061511768503621569, 0.061511768503621569, 0.0, -0.87697646299275689, 0.0},
    {0.0040155050228577382, 0.0080310100457154764, 0.0040155050228577382, -1.8614084445321082, 0.87747046462353917}
};

inline constexpr IndependentBiquadCoeffs LP_24_SECTIONS[2]{
    {0.0038172458174315226, 0.0076344916348630451, 0.0038172458174315226, -1.7695043485128368, 0.78477333178256292},
    {0.0040740687198803248, 0.0081481374397606495, 0.0040740687198803248, -1.8885559538890460, 0.90485222876856730}
};

inline constexpr IndependentBiquadCoeffs LP_36_SECTIONS[3]{
    {0.0037986417971591722, 0.0075972835943183444, 0.0037986417971591722, -1.7608803571991478, 0.77607492438778447},
    {0.0039161266605473692, 0.0078322533210947384, 0.0039161266605473692, -1.8153410827045682, 0.83100558934675750},
    {0.0041377839465397788, 0.0082755678930795575, 0.0041377839465397788, -1.9180914818672381, 0.93464261765339729}
};

inline constexpr IndependentBiquadCoeffs LP_48_SECTIONS[4]{
    {0.0037921102995535682, 0.0075842205991071363, 0.0037921102995535682, -1.7578526471777918, 0.77302108837600592},
    {0.0038587813233042080, 0.0077175626466084160, 0.0038587813233042080, -1.7887583504227402, 0.80419347571595712},
    {0.0039883483793519093, 0.0079766967587038187, 0.0039883483793519093, -1.8488198397964271, 0.86477323331383471},
    {0.0041713484409052325, 0.0083426968818104651, 0.0041713484409052325, -1.9336504795257301, 0.95033587328935099}
};

}  // namespace ref_constants

// O2: Independent scalar TDF-II sample & cascade reference processor
struct IndependentTdf2State final {
    double s1{0.0};
    double s2{0.0};

    double process_sample(double x, const IndependentBiquadCoeffs& c) noexcept
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }

    void reset() noexcept
    {
        s1 = 0.0;
        s2 = 0.0;
    }
};

class IndependentCascadeTdf2State final {
public:
    IndependentCascadeTdf2State() = default;

    explicit IndependentCascadeTdf2State(std::vector<IndependentBiquadCoeffs> sections)
        : sections_(std::move(sections))
        , states_(sections_.size())
    {
    }

    double process_sample(double x) noexcept
    {
        double current = x;
        for (std::size_t i = 0; i < sections_.size(); ++i) {
            current = states_[i].process_sample(current, sections_[i]);
        }
        return current;
    }

    void process_block(std::span<const double> in, std::span<double> out) noexcept
    {
        const std::size_t count = std::min(in.size(), out.size());
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = process_sample(in[i]);
        }
    }

    void reset() noexcept
    {
        for (auto& s : states_) {
            s.reset();
        }
    }

private:
    std::vector<IndependentBiquadCoeffs> sections_;
    std::vector<IndependentTdf2State> states_;
};

// O3: Independent analytic transfer oracle
inline std::complex<double> biquad_transfer_function(
    const IndependentBiquadCoeffs& c,
    double frequency_hz,
    double sample_rate_hz)
{
    const double w = 2.0 * std::numbers::pi * frequency_hz / sample_rate_hz;
    const std::complex<double> j(0.0, 1.0);
    const std::complex<double> z1 = std::exp(-j * w);
    const std::complex<double> z2 = std::exp(-j * 2.0 * w);

    const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return num / den;
}

inline std::complex<double> cascade_transfer_function(
    std::span<const IndependentBiquadCoeffs> sections,
    double frequency_hz,
    double sample_rate_hz)
{
    std::complex<double> h(1.0, 0.0);
    for (const auto& c : sections) {
        h *= biquad_transfer_function(c, frequency_hz, sample_rate_hz);
    }
    return h;
}

// O4: Rendered signal verification helper (RMS difference)
inline double compute_rms_diff(std::span<const double> a, std::span<const double> b)
{
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return std::sqrt(sum / static_cast<double>(a.size()));
}

inline double compute_max_abs_diff(std::span<const double> a, std::span<const double> b)
{
    if (a.size() != b.size()) return 0.0;
    double max_diff = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double diff = std::abs(a[i] - b[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    return max_diff;
}

}  // namespace rgsml::tests::oracles
