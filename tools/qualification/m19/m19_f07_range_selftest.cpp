// M19 F07 independent numerical adversaries: native C++20 actual candidate.
// Mathematical expectations do NOT import the M19 reduction code.
// The private diagnostic is intentionally exercised at 44.1 kHz:
// no accepted non-48k K-weighting bank, hence no misleading LUFS assertion.
#include "internal/m19_basic_level_candidate.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace m19=rgsml::analysis::internal::m19;
using m19::BasicLevelPreproductionSnapshot;

void require(bool truth)
{
    if (!truth) throw std::runtime_error("M19_F07_NUMERICAL_ADVERSARY_FAILURE");
}
void near(double a,double b,double tol=2e-12)
{
    require(std::isfinite(a) && std::isfinite(b) &&
            std::abs(a-b)<=tol);
}
[[nodiscard]] BasicLevelPreproductionSnapshot
run(const std::vector<double>& left,
    const std::vector<double>& right={},
    const std::vector<std::size_t>& cuts={4096U})
{
    require(!left.empty() && !cuts.empty() &&
            (right.empty() || right.size()==left.size()));
    m19::BasicLevelPreproduction engine(
        44100U,right.empty()?1U:2U,12345U,left.size());
    std::size_t offset=0,index=0;
    while (offset<left.size()) {
        const std::size_t length=
            std::min(cuts[index%cuts.size()],left.size()-offset);
        require(length>0);
        auto lhs=std::span<const double>(left).subspan(offset,length);
        auto rhs=right.empty()?std::span<const double>{}
            :std::span<const double>(right).subspan(offset,length);
        require(engine.push(lhs,rhs));
        offset+=length;
        ++index;
    }
    auto result=engine.finalize();
    require(result.has_value());
    require(!result->integrated_lufs);
    require(result->loudness_state==
        m19::LoudnessPreproductionState::unsupported_k_weighting_rate);
    require(!result->true_peak_db_tp);
    return *result;
}

int main()
{
    constexpr std::size_t n=4097U;
    // Exact dyadic adversary from 2026-10-04 F07:
    // sum of squares = 1 + 4096 * 2^-60 = 1 + 2^-48.
    // Naive left-to-right binary64 drops the 2^-60 terms one-by-one.
    const double small=std::ldexp(1.0,-30);
    std::vector<double> oneFirst(n,small),oneLast(n,small);
    oneFirst[0]=1.0;
    oneLast[n-1U]=1.0;
    const auto oraclePower=(1.0+std::ldexp(1.0,-48))
                           /static_cast<double>(n);
    const auto expected=10.0*std::log10(oraclePower);
    const auto front=run(oneFirst,{}, {1U,13U,7U,127U});
    const auto back=run(oneLast,{}, {4096U,1U});
    require(front.full_scope_rms_dbfs && back.full_scope_rms_dbfs);
    require(front.sample_peak_crest_db && back.sample_peak_crest_db);
    near(*front.full_scope_rms_dbfs,expected,1e-14);
    near(*back.full_scope_rms_dbfs,expected,1e-14);
    near(*front.full_scope_rms_dbfs,*back.full_scope_rms_dbfs,1e-14);
    const auto base=run([&]{
        std::vector<double> v(n,0.0);v[0]=1.0;return v;
    }());
    require(base.full_scope_rms_dbfs);
    // This must be distinguishable from the naive sum of just 1.0.
    require(*front.full_scope_rms_dbfs >
            *base.full_scope_rms_dbfs+4e-15);
    require(*back.full_scope_rms_dbfs >
            *base.full_scope_rms_dbfs+4e-15);
    // Repartitioning is NOT a new reduction schedule.
    const auto uninterrupted=run(oneFirst,{}, {n});
    require(front.full_scope_rms_dbfs==uninterrupted.full_scope_rms_dbfs);
    require(front.sample_peak_crest_db==uninterrupted.sample_peak_crest_db);
    near(*front.sample_peak_crest_db,-expected,1e-14);

    // "Peak scale arrives late": normalized energy and its compensation
    // must both rescale when an incoming peak exceeds the previous peak.
    auto lateGain=oneLast;
    lateGain[n-1U]=2.0;
    const auto late=run(lateGain,{}, {1U,3U,4096U});
    const double exactPower=(4.0+std::ldexp(1.0,-48))/n;
    near(*late.full_scope_rms_dbfs,10.0*std::log10(exactPower),2e-14);
    near(*late.sample_peak_dbfs,20.0*std::log10(2.0),2e-14);

    // Range: direct square underflows, but log-amplitude RMS is defined;
    // no -Inf, NaN, clamping, or made-up "silence".
    constexpr std::array<int,5> exponents{-600,-510,0,510,512};
    for (const int exponent: exponents) {
        const double amplitude=std::ldexp(1.0,exponent);
        const std::vector<double> block(64U,amplitude);
        auto a=run(block,{}, {1U});
        auto b=run(block,block, {7U,2U,13U});
        require(a.sample_peak_dbfs && a.full_scope_rms_dbfs
                && a.sample_peak_crest_db);
        const double db=20.0*std::log10(amplitude);
        near(*a.sample_peak_dbfs,db,2e-12);
        near(*a.full_scope_rms_dbfs,db,2e-12);
        near(*a.sample_peak_crest_db,0.0,2e-12);
        near(*b.full_scope_rms_dbfs,db,2e-12);
        near(*b.sample_peak_dbfs,db,2e-12);
        near(*b.sample_peak_crest_db,0.0,2e-12);
    }
    // The smallest representable positive binary64, and the largest:
    // their square is under-/overflowing in double; the level isn't.
    const std::array<double,2> extremes{
        (std::numeric_limits<double>::denorm_min)(),
        (std::numeric_limits<double>::max)()};
    for (double magnitude: extremes) {
        auto result=run(std::vector<double>(64U,magnitude),{}, {11U});
        const double expectedDb=20.0*std::log10(magnitude);
        near(*result.sample_peak_dbfs,expectedDb,2e-12);
        near(*result.full_scope_rms_dbfs,expectedDb,2e-12);
        near(*result.sample_peak_crest_db,0.0,2e-12);
    }
    // Finite waveform + outside zero: crest is Sample-Peak-based,
    // NOT a True Peak /RMS ratio.
    const std::vector<double> single{1.0,0.0,0.0,0.0};
    const auto pulse=run(single,{}, {1U});
    near(*pulse.full_scope_rms_dbfs,-6.020599913279623,1e-12);
    near(*pulse.sample_peak_dbfs,0.0,1e-12);
    near(*pulse.sample_peak_crest_db,6.020599913279623,1e-12);

    // NaN/Inf cannot hide in either channel or in an incomplete tail.
    for (const double poison:{
            (std::numeric_limits<double>::quiet_NaN)(),
            (std::numeric_limits<double>::infinity)(),
            -(std::numeric_limits<double>::infinity)()}) {
        auto invalid=single;
        invalid.back()=poison;
        m19::BasicLevelPreproduction engine(44100U,1U,0U,invalid.size());
        require(!engine.push(invalid));
        require(!engine.finalize());
        auto right=single;
        right[1U]=poison;
        m19::BasicLevelPreproduction stereo(44100U,2U,0U,right.size());
        require(!stereo.push(single,right));
        require(!stereo.finalize());
    }
    // Signed zeros are exact silence, never fake zero logarithms or crest.
    const auto zero=run({0.0,-0.0,0.0,-0.0});
    require(!zero.full_scope_rms_dbfs && !zero.sample_peak_dbfs &&
            !zero.sample_peak_crest_db);
    return 0;
}
