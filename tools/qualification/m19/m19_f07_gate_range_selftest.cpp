// M19 F07 / F04: independent gate sufficient-statistic adversarial matrix.
// This is a PRIVATE numerical diagnostic, NOT a raw-PCM realizability
// assertion, not a frozen QTP, and not official Gate G evidence.
#include "internal/m19_basic_level_candidate.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace m19 = rgsml::analysis::internal::m19;

void require(bool value)
{
    if (!value) throw std::runtime_error("M19_F07_GATE_RANGE_FAILURE");
}

void close(double value, double oracle, double tolerance = 2e-11)
{
    require(std::isfinite(value) && std::isfinite(oracle));
    require(std::abs(value-oracle) <= tolerance);
}

void check(const std::vector<double>& q,
           std::size_t active, std::size_t retained, double loudness)
{
    const auto result = m19::gate_complete_400ms_block_powers(q);
    require(result.has_value());
    require(result->absolute_count == active);
    require(result->final_count == retained);
    require(result->integrated_lufs.has_value());
    close(*result->integrated_lufs,loudness);

    // Mathematical membership cannot depend on input list order, unlike
    // an improperly order-sensitive floating-point reduction.
    auto reversed = q;
    std::reverse(reversed.begin(),reversed.end());
    const auto reverseResult = m19::gate_complete_400ms_block_powers(reversed);
    require(reverseResult.has_value());
    require(reverseResult->absolute_count==active);
    require(reverseResult->final_count==retained);
    require(reverseResult->integrated_lufs.has_value());
    close(*reverseResult->integrated_lufs,loudness);
}

int main()
{
    const double largest = (std::numeric_limits<double>::max)();
    const double justAboveOne = std::nextafter(1.0,2.0);
    const auto integrated=[](double q) {
        return -0.691 + 10.0*std::log10(q);
    };

    // Previously naive sumA/sumR overflowed, although the actual MEANS
    // and LUFS values were finite and unambiguous.
    check({1e308,1e308},2U,2U,integrated(1e308));
    check({largest,largest},2U,2U,integrated(largest));
    check({largest,largest/2.0},2U,2U,integrated(largest*0.75));
    check(std::vector<double>(1000U,1e308),1000U,1000U,integrated(1e308));

    // Only very loud block remains after -10-LU relative gate. A small
    // but absolute-active q does NOT become part of the final mean.
    check({1e308,1e-6},2U,1U,integrated(1e308));
    check({1e308,1e306},2U,1U,integrated(1e308));
    check({1e308,1e307},2U,2U,integrated(5.5e307));

    // F04 strict relative equality: no '>=' and no display rounding.
    check({19.0,1.0},2U,1U,integrated(19.0));
    check({19.0,justAboveOne},2U,2U,integrated(10.0));

    // Pre-audit [901, 1*99] stress: exact equality excludes 99 blocks;
    // each nextafter just ABOVE equality must be counted. This produces
    // a >19 LU membership discontinuity from a one-ULP input change.
    std::vector<double> stress(100U,1.0);
    stress[0]=901.0;
    check(stress,100U,1U,integrated(901.0));
    std::fill(stress.begin()+1,stress.end(),justAboveOne);
    check(stress,100U,100U,integrated(10.0));

    // Absolute gate exactly -70 LKFS, next representable positive power,
    // and silent blocks: strict membership, no epsilon/fabricated -Inf.
    const double threshold = std::pow(10.0,(-70.0+0.691)/10.0);
    const std::vector<double> around{
        std::nextafter(threshold,0.0), threshold,
        std::nextafter(threshold,1.0)};
    const auto a = m19::gate_complete_400ms_block_powers(around);
    require(a.has_value() && a->absolute_count==1U && a->final_count==1U);
    const auto empty=m19::gate_complete_400ms_block_powers(
        std::vector<double>{0.0,-0.0,0.0});
    require(empty.has_value() && empty->absolute_count==0U &&
            empty->final_count==0U && !empty->integrated_lufs);
    require(!m19::gate_complete_400ms_block_powers(
        std::vector<double>{1.0,-1.0}));
    require(!m19::gate_complete_400ms_block_powers(
        std::vector<double>{1.0,(std::numeric_limits<double>::infinity)()}));
    require(!m19::gate_complete_400ms_block_powers(
        std::vector<double>{1.0,(std::numeric_limits<double>::quiet_NaN)()}));

    // All positive means must remain finite. This is a *private*
    // numerical requirement, not authority to cast invalid q from a
    // 48-kHz K-filter overflow into a valid product result.
    std::vector<double> nearLimit(10000U,largest/2.0);
    check(nearLimit,10000U,10000U,integrated(largest/2.0));
    return 0;
}
