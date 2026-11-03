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

using rgsml::analysis::internal::m19::BasicLevelPreproduction;
using rgsml::analysis::internal::m19::BasicLevelPreproductionSnapshot;
using rgsml::analysis::internal::m19::LoudnessPreproductionState;

namespace {
void require(bool value) {
    if (!value) { throw std::runtime_error("M19_PREPRODUCTION_TEST_FAILURE"); }
}
void near(double actual, double expected, double epsilon = 0.00001) {
    require(std::isfinite(actual) && std::abs(actual - expected) <= epsilon);
}
BasicLevelPreproductionSnapshot run(const std::vector<double>& left,
                                     const std::vector<double>& right,
                                     std::uint32_t hz,
                                     std::uint64_t origin,
                                     const std::vector<std::size_t>& cuts)
{
    BasicLevelPreproduction m(hz, right.empty() ? 1U : 2U, origin, left.size());
    std::size_t begin = 0U, index = 0U;
    while (begin < left.size()) {
        const std::size_t count = std::min(
            cuts[index % cuts.size()], left.size() - begin);
        require(count > 0U);
        require(m.push(std::span<const double>(left).subspan(begin, count),
                       std::span<const double>(right).empty()
                           ? std::span<const double>{}
                           : std::span<const double>(right).subspan(begin, count)));
        begin += count;
        ++index;
    }
    auto output = m.finalize();
    require(output.has_value());
    require(!m.finalize().has_value());
    return *output;
}
void check_chunk_equal(const BasicLevelPreproductionSnapshot& a,
                       const BasicLevelPreproductionSnapshot& b) {
    require(a.integrated_lufs == b.integrated_lufs);
    require(a.full_scope_rms_dbfs == b.full_scope_rms_dbfs);
    require(a.sample_peak_dbfs == b.sample_peak_dbfs);
    require(a.sample_peak_crest_db == b.sample_peak_crest_db);
    require(a.loudness_state == b.loudness_state);
}
} // namespace

int main() {
    constexpr std::uint64_t origin = 12345U;
    const std::vector<std::size_t> entire{50000U};
    const std::vector<std::size_t> ragged{1U,7U,2U,31U,3U,127U,64U,511U};

    // Independent accepted zero-state 48-kHz mono DC first-block oracle.
    const std::vector<double> dc(19200U, 1.0);
    auto direct = run(dc, {}, 48000U, origin, entire);
    auto chunks = run(dc, {}, 48000U, origin, ragged);
    check_chunk_equal(direct, chunks);
    require(direct.integrated_lufs.has_value());
    near(*direct.integrated_lufs, -26.2768703195911, 0.00002);
    near(*direct.full_scope_rms_dbfs, 0.0);
    near(*direct.sample_peak_dbfs, 0.0);
    near(*direct.sample_peak_crest_db, 0.0);
    require(!direct.true_peak_db_tp.has_value()); // M16 owns TP.

    // Two identical channels DOUBLE the weighted K power but NOT full-scope
    // RMS or sample peak; no unrequested stereo metric is calculated.
    auto stereo = run(dc, dc, 48000U, origin, ragged);
    near(*stereo.integrated_lufs - *direct.integrated_lufs,
         3.010299956639812, 0.00002);
    near(*stereo.full_scope_rms_dbfs, 0.0);
    near(*stereo.sample_peak_dbfs, 0.0);
    near(*stereo.sample_peak_crest_db, 0.0);

    // Accepted impulse oracle, including the causal zero-state transient.
    auto impulse = std::vector<double>(19200U, 0.0);
    impulse[0] = 1.0;
    auto pulse = run(impulse, {}, 48000U, 96000U, ragged);
    near(*pulse.integrated_lufs, -39.6949569766218, 0.00002);
    near(*pulse.sample_peak_dbfs, 0.0);
    near(*pulse.full_scope_rms_dbfs, -10.0 * std::log10(19200.0));
    near(*pulse.sample_peak_crest_db, 10.0 * std::log10(19200.0));

    // Absence is not -Inf/NaN, even with a full 400-ms silence block.
    auto silence = run(std::vector<double>(19200U, 0.0), {},
                       48000U, 0U, ragged);
    require(!silence.integrated_lufs && !silence.full_scope_rms_dbfs
            && !silence.sample_peak_dbfs && !silence.sample_peak_crest_db);
    require(silence.loudness_state ==
            LoudnessPreproductionState::no_absolute_gated_blocks);

    // 400 ms support is exact: 19199 lacks a full block, 19201 has one.
    auto tooShort = run(std::vector<double>(19199U, 0.5), {},
                        48000U, origin, ragged);
    require(tooShort.loudness_state ==
            LoudnessPreproductionState::insufficient_duration);
    near(*tooShort.full_scope_rms_dbfs, 20.0*std::log10(0.5));
    near(*tooShort.sample_peak_crest_db, 0.0);
    auto tail = run(std::vector<double>(19201U, 1.0), {},
                    48000U, origin, ragged);
    require(tail.integrated_lufs.has_value());
    near(*tail.integrated_lufs, *direct.integrated_lufs, 0.00002);

    // Outside logical scope must not influence the raw PCM selected by caller.
    std::vector<double> resource(19202U, 10000.0);
    std::copy(dc.begin(), dc.end(), resource.begin()+1);
    auto selected = run(std::vector<double>(resource.begin()+1,
                                           resource.end()-1), {},
                        48000U, origin, ragged);
    check_chunk_equal(selected, direct);

    // >0-dBFS peaks are valid; no clamp or DC removal.
    auto hot = run(std::vector<double>(19200U, 2.0), {},
                   48000U, origin, ragged);
    near(*hot.sample_peak_dbfs, 20.0*std::log10(2.0));
    near(*hot.full_scope_rms_dbfs, 20.0*std::log10(2.0));
    near(*hot.sample_peak_crest_db, 0.0);

    // Five other rates must NOT pretend to have certified K banks.
    for (std::uint32_t hz : {44100U, 88200U, 96000U, 176400U, 192000U}) {
        auto out = run(std::vector<double>(2000U, 0.25), {}, hz, origin, ragged);
        require(out.loudness_state ==
                LoudnessPreproductionState::unsupported_k_weighting_rate);
        require(!out.integrated_lufs);
        near(*out.full_scope_rms_dbfs, 20.0*std::log10(0.25));
        near(*out.sample_peak_dbfs, 20.0*std::log10(0.25));
    }
    // Independent 2026-10-04 F04 strict relative-gating adversary:
    // q=[19,1] has meanA=10, threshold qR=1 EXACTLY. Equality is EXCLUDED.
    // The low block's next representable positive power MUST be included.
    {
        const std::array<double,2> eq{19.0,1.0};
        auto a = rgsml::analysis::internal::m19::
            gate_complete_400ms_block_powers(eq);
        require(a.has_value());
        require(a->absolute_count == 2U && a->final_count == 1U);
        require(a->integrated_lufs.has_value());
        near(*a->integrated_lufs, -0.691 + 10.0*std::log10(19.0), 1e-12);

        const std::array<double,2> next{
            19.0, std::nextafter(1.0, 2.0)};
        auto b = rgsml::analysis::internal::m19::
            gate_complete_400ms_block_powers(next);
        require(b.has_value());
        require(b->absolute_count == 2U && b->final_count == 2U);
        require(b->integrated_lufs.has_value());
        near(*b->integrated_lufs, -0.691+10.0*std::log10(10.0), 1e-12);
        require(*a->integrated_lufs > *b->integrated_lufs + 2.7);

        const double absolute = std::pow(10.0,(-70.0 + 0.691)/10.0);
        const std::array<double,3> boundary{
            std::nextafter(absolute, 0.0),
            absolute, std::nextafter(absolute, 1.0)};
        auto c = rgsml::analysis::internal::m19::
            gate_complete_400ms_block_powers(boundary);
        require(c.has_value());
        require(c->absolute_count == 1U && c->final_count == 1U);
        require(c->integrated_lufs.has_value());
        auto silencePowers = rgsml::analysis::internal::m19::
            gate_complete_400ms_block_powers(std::array<double,2>{0.0,0.0});
        require(silencePowers.has_value());
        require(!silencePowers->integrated_lufs);
        require(silencePowers->absolute_count == 0U);

        require(!rgsml::analysis::internal::m19::
            gate_complete_400ms_block_powers(std::array<double,2>{1.0,-1.0}));
        require(!rgsml::analysis::internal::m19::
            gate_complete_400ms_block_powers(std::array<double,2>{
                1.0,(std::numeric_limits<double>::infinity)()}));
        // F07: mathematical mean is representable even when the
        // unnormalized *sum* of two valid block powers overflows binary64.
        auto huge = rgsml::analysis::internal::m19::
            gate_complete_400ms_block_powers(std::array<double,2>{1e308,1e308});
        require(huge.has_value());
        require(huge->absolute_count == 2U && huge->final_count == 2U);
        require(huge->integrated_lufs.has_value());
        near(*huge->integrated_lufs,-0.691+10.0*std::log10(1e308),1e-10);
    }

    // Independent bounded K-scope re-signoff: 12 feeder partitions per
    // family, as frozen in the Oct-04 addendum, at nonzero scope origin.
    // The *same* chronological sample sequence must yield identical bit-level
    // static metrics regardless of feeder boundaries or whole/ragged blocks.
    {
        constexpr std::size_t total=33617U;
        constexpr std::uint64_t originA=12345U;
        constexpr std::uint64_t originB=96000U;
        const std::array<std::vector<std::size_t>,12> schedules{
            std::vector<std::size_t>{total},
            std::vector<std::size_t>{1U},
            std::vector<std::size_t>{2U},
            std::vector<std::size_t>{3U},
            std::vector<std::size_t>{7U},
            std::vector<std::size_t>{64U},
            std::vector<std::size_t>{127U},
            std::vector<std::size_t>{256U},
            std::vector<std::size_t>{511U},
            std::vector<std::size_t>{1024U},
            std::vector<std::size_t>{4096U},
            std::vector<std::size_t>{1U,7U,2U,31U,3U,127U,64U,511U}};
        std::vector<double> steady(total,1.0);
        std::vector<double> tone(total,0.0);
        std::vector<double> transient(total,0.0);
        std::vector<double> zeros(total,0.0);
        std::vector<double> opposite(total,-1.0);
        transient[0]=1.0;
        constexpr double twopi=6.283185307179586476925286766559;
        for (std::size_t i=0;i<total;++i) {
            tone[i]=0.125*std::sin(twopi*1000.0*
                                   static_cast<double>(i)/48000.0);
        }
        struct Family {
            const std::vector<double>* left;
            const std::vector<double>* right;
        };
        const std::array<Family,5> families{{
            {&steady,nullptr},
            {&tone,nullptr},
            {&transient,nullptr},
            {&steady,&opposite},
            {&zeros,nullptr}
        }};
        for (const auto& family : families) {
            const auto& l=*family.left;
            const auto& r=family.right==nullptr
                ? std::vector<double>{} : *family.right;
            const auto reference=run(l,r,48000U,originA,{total});
            for (const auto& plan : schedules) {
                auto got=run(l,r,48000U,originA,plan);
                check_chunk_equal(got,reference);
                require(got.scope_frames==total &&
                        got.scope_begin_frame==originA);
            }
            const auto relocated=run(l,r,48000U,originB,schedules.back());
            check_chunk_equal(relocated,reference);
        }
        const auto toneResult=run(tone,{},48000U,originA,schedules.back());
        require(toneResult.integrated_lufs.has_value());
        near(*toneResult.integrated_lufs,-21.0654702260936,0.0001);
        auto dc33617=run(steady,{},48000U,originA,schedules.back());
        require(dc33617.integrated_lufs.has_value());
        near(*dc33617.integrated_lufs,-26.2768703195911,0.00002);
        const auto zero33617=run(zeros,{},48000U,originA,schedules.back());
        require(!zero33617.integrated_lufs);
        require(zero33617.loudness_state==
                LoudnessPreproductionState::no_absolute_gated_blocks);

        // A valid LUFS first block + later unfinished tail is still one
        // logical analysis scope; a late NaN must invalidate that whole scope.
        auto lateCorruption=steady;
        lateCorruption.back()=(std::numeric_limits<double>::infinity)();
        BasicLevelPreproduction badTail(48000U,1U,originA,total);
        require(!badTail.push(lateCorruption));
        require(!badTail.finalize().has_value());
    }

    // Invalid data inside the scope: prevalidate the WHOLE chunk before state.
    BasicLevelPreproduction m(48000U,1U,0U,19200U);
    auto bad = dc;
    bad.back() = (std::numeric_limits<double>::quiet_NaN)();
    require(!m.push(bad));
    require(m.push(dc));
    require(m.finalize().has_value());

    BasicLevelPreproduction wrong(48000U, 2U, 0U, 19200U);
    require(!wrong.push(dc));
    require(wrong.push(dc,dc));
    require(wrong.finalize().has_value());
    BasicLevelPreproduction empty(48000U,1U,0U,0U);
    require(!empty.finalize());
    BasicLevelPreproduction overflow(
        48000U,1U,(std::numeric_limits<std::uint64_t>::max)(),19200U);
    require(!overflow.push(dc));
    return 0;
}
