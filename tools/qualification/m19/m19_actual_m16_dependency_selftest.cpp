// M19 preproduction dependency proof against the *existing* public M16 API.
// This does not claim M16 is qualified or provide an M19 region-TP adapter.
// No second oversampler, FIR, or alternative true peak estimate exists here.
#include "internal/m19_basic_level_candidate.hpp"
#include "internal/m19_audio_scope_adapter_candidate.hpp"
#include <rgsml/analysis/true_peak_measurement.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/core/frame_time.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

using rgsml::analysis::TruePeakMeasurement;
using rgsml::analysis::TruePeakQualificationStatus;
using rgsml::analysis::measure_true_peak_full_resource;
using rgsml::analysis::internal::m19::BasicLevelPreproduction;
using rgsml::audio::AudioBuffer;
using rgsml::audio::AudioFormat;
using rgsml::audio::ChannelLayout;
using rgsml::audio::FrameDomainId;

static_assert(!std::is_default_constructible_v<TruePeakMeasurement>);
static_assert(std::is_same_v<
    decltype(std::declval<const TruePeakMeasurement&>().qualification_status()),
    TruePeakQualificationStatus>);

namespace {
void require(bool v) {
    if (!v) throw std::runtime_error("M19_M16_PUBLIC_API_DEPENDENCY_FAILURE");
}

rgsml::core::Result<AudioBuffer> makeBuffer(
    std::int64_t hz, ChannelLayout layout, std::int64_t origin,
    const std::vector<double>& a, const std::vector<double>& b = {},
    FrameDomainId domain = FrameDomainId::SOURCE_PROCESSING_RATE)
{
    auto rate = rgsml::core::SampleRate::create(hz);
    if (!rate) return rgsml::core::Result<AudioBuffer>::failure(*rate.error());
    auto format = AudioFormat::create(*rate.value(), layout);
    if (!format) return rgsml::core::Result<AudioBuffer>::failure(*format.error());
    auto frameCount = rgsml::core::FrameCount::create(
        static_cast<std::int64_t>(a.size()));
    if (!frameCount) return rgsml::core::Result<AudioBuffer>::failure(*frameCount.error());
    auto buf = AudioBuffer::create(*format.value(), domain,
                                   rgsml::core::FrameIndex{origin}, *frameCount.value());
    if (!buf) return buf;
    auto view = buf.value()->mutable_view();
    auto l = view.channel(0);
    if (!l) return rgsml::core::Result<AudioBuffer>::failure(*l.error());
    std::copy(a.begin(), a.end(), l.value()->begin());
    if (layout == ChannelLayout::STEREO_LR) {
        require(a.size() == b.size());
        auto r = view.channel(1);
        if (!r) return rgsml::core::Result<AudioBuffer>::failure(*r.error());
        std::copy(b.begin(), b.end(), r.value()->begin());
    }
    return buf;
}
}

int main()
{
    // M16 actual public whole-resource call at NONZERO resource origin. The
    // M19 scope matches exactly the whole owning resource, not an arbitrary
    // regional subview, preserving M16's currently frozen call boundary.
    const std::vector<double> dc(19200U,1.0);
    auto pcm=makeBuffer(48000,ChannelLayout::MONO_C,12345,dc);
    require(pcm.has_value());
    const auto m16=measure_true_peak_full_resource(*pcm.value());
    require(m16.has_value());
    require(m16.value()->qualification_status() ==
            TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
    require(m16.value()->sample_rate_hz()==48000);
    require(m16.value()->channel_count()==1U);
    require(m16.value()->global_db_tp().has_value());
    require(m16.value()->true_peak_policy_id()==
            std::string_view{"rgsml.analysis.true-peak.itu4x/1.0.0"});
    require(m16.value()->result_schema_id()==
            std::string_view{"rgsml.analysis.true-peak.result/1.0.0"});
    require(m16.value()->filter_asset_sha256()==std::string_view{
            "D1DF3373D3C5A82A3E4CFE3576BDDF97E153F3281FF9287554E024FA6CB0D316"});
    auto channel=pcm.value()->view().channel(0);
    require(channel.has_value());
    BasicLevelPreproduction m19(48000U,1U,12345U,19200U);
    require(m19.push(*channel.value()));
    const auto snapshot=m19.finalize();
    require(snapshot.has_value());
    require(snapshot->scope_begin_frame==12345U &&
            snapshot->scope_frames==19200U);
    require(snapshot->integrated_lufs.has_value());
    require(!snapshot->true_peak_db_tp.has_value());
    // M19 MUST NOT silently accept the currently UNQUALIFIED M16 numeric
    // payload as a five-metric qualified product result.
    require(m16.value()->qualification_status() !=
            TruePeakQualificationStatus::QUALIFIED);

    // Canonical mono/stereo M16, six-rate behavior remains independently
    // callable. M19 never substitutes its own TP detector.
    const std::vector<double> left{0.25,1.0,0.0,-0.125};
    const std::vector<double> right{-0.5,-2.0,0.125,0.75};
    for (std::int64_t rate : {44100,48000,88200,96000,176400,192000}) {
        auto stereo=makeBuffer(rate,ChannelLayout::STEREO_LR,0,left,right);
        require(stereo.has_value());
        const auto peak=measure_true_peak_full_resource(*stereo.value());
        require(peak.has_value());
        require(peak.value()->sample_rate_hz()==rate);
        require(peak.value()->channel_count()==2U);
        require(peak.value()->global_db_tp().has_value());
        require(peak.value()->qualification_status()==
                TruePeakQualificationStatus::SUPPORTED_UNQUALIFIED);
    }
    // Actual M16 refuses invalid PCM, and M19 refuses the same selected
    // input WITHOUT populating a falsely valid TP field.
    auto bad=left;
    bad[2]=(std::numeric_limits<double>::quiet_NaN)();
    auto invalid=makeBuffer(48000,ChannelLayout::MONO_C,0,bad);
    require(invalid.has_value());
    const auto failed=measure_true_peak_full_resource(*invalid.value());
    require(!failed);
    require(failed.error()->code()==rgsml::core::ErrorCode::InvalidAudioSample);
    BasicLevelPreproduction candidate(48000U,1U,0U,bad.size());
    require(!candidate.push(bad));

    // The actual canonical AudioBufferView subview isolates a NONZERO selected
    // scope. Outside values may be huge, NaN, or Inf; they are NEVER read by
    // M19. The K filter resets to zero at frame 48000, not resource frame 0.
    {
        constexpr std::size_t scopeBegin=48000U;
        constexpr std::size_t scopeLength=19200U;
        std::vector<double> envelope(scopeBegin+scopeLength+17U,1000000.0);
        std::fill(envelope.begin()+scopeBegin,
                  envelope.begin()+scopeBegin+scopeLength,1.0);
        envelope[scopeBegin-1U]=(std::numeric_limits<double>::quiet_NaN)();
        envelope[scopeBegin+scopeLength]=(std::numeric_limits<double>::infinity)();
        auto owner=makeBuffer(48000,ChannelLayout::MONO_C,0,envelope);
        require(owner.has_value());
        const auto frameCount=rgsml::core::FrameCount::create(
            static_cast<std::int64_t>(scopeLength));
        require(frameCount.has_value());
        auto selected=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),
                rgsml::core::FrameIndex{static_cast<std::int64_t>(scopeBegin)},
                *frameCount.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
        require(selected.has_value());
        require(selected.value()->scope_begin_frame==scopeBegin);
        require(selected.value()->scope_frames==scopeLength);
        require(selected.value()->integrated_lufs.has_value());
        require(std::abs(*selected.value()->integrated_lufs-
                         (-26.2768703195911)) < 0.00002);
        require(selected.value()->full_scope_rms_dbfs==0.0);
        require(selected.value()->sample_peak_dbfs==0.0);
        require(selected.value()->sample_peak_crest_db==0.0);
        require(!selected.value()->true_peak_db_tp);
        // SAME selection is repeatable and does not mutate the resource.
        auto repeated=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),
                rgsml::core::FrameIndex{static_cast<std::int64_t>(scopeBegin)},
                *frameCount.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
        require(repeated.has_value());
        require(repeated.value()->integrated_lufs==selected.value()->integrated_lufs);
        auto untouched=owner.value()->view().channel(0U);
        require(untouched.has_value());
        require(std::isnan((*untouched.value())[scopeBegin-1U]));
        require(std::isinf((*untouched.value())[scopeBegin+scopeLength]));
        // The M16 *whole-resource* API must not be misused for regional TP.
        auto forbiddenRegionalSubstitute=measure_true_peak_full_resource(*owner.value());
        require(!forbiddenRegionalSubstitute);
        require(forbiddenRegionalSubstitute.error()->code()==
                rgsml::core::ErrorCode::InvalidAudioSample);

        auto wrongDomain=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),
                rgsml::core::FrameIndex{static_cast<std::int64_t>(scopeBegin)},
                *frameCount.value(),FrameDomainId::OUTPUT_RATE);
        require(!wrongDomain);
        require(wrongDomain.error()->code()==
                rgsml::core::ErrorCode::InvalidFrameRange);
        auto outsideCount = rgsml::core::FrameCount::create(
            static_cast<std::int64_t>(scopeLength+18U));
        require(outsideCount.has_value());
        auto outside=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),
                rgsml::core::FrameIndex{static_cast<std::int64_t>(scopeBegin)},
                *outsideCount.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
        require(!outside);
        auto negative=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),
                rgsml::core::FrameIndex{-1},
                *frameCount.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
        require(!negative);
        auto zeroCount = rgsml::core::FrameCount::create(0);
        require(zeroCount.has_value());
        auto emptied=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),rgsml::core::FrameIndex{0},
                *zeroCount.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
        require(!emptied);
        // A nonfinite value INSIDE the scope is categorically rejected.
        auto corrupted=envelope;
        corrupted[scopeBegin+101U]=(std::numeric_limits<double>::quiet_NaN)();
        auto invalidScopeOwner=makeBuffer(48000,ChannelLayout::MONO_C,0,corrupted);
        require(invalidScopeOwner.has_value());
        auto invalidScope=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                invalidScopeOwner.value()->view(),
                rgsml::core::FrameIndex{static_cast<std::int64_t>(scopeBegin)},
                *frameCount.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
        require(!invalidScope);
        require(invalidScope.error()->code()==
                rgsml::core::ErrorCode::InvalidAudioSample);

        // OUTPUT_RATE owns its own frame-domain identity; M19 never silently
        // switches to SOURCE or DEVICE rates.
        auto output=makeBuffer(48000,ChannelLayout::MONO_C,0,dc,{},
                               FrameDomainId::OUTPUT_RATE);
        require(output.has_value());
        auto matchingOutput=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                output.value()->view(),rgsml::core::FrameIndex{0},
                *frameCount.value(),FrameDomainId::OUTPUT_RATE);
        require(matchingOutput.has_value());
        auto mismatchedSource=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                output.value()->view(),rgsml::core::FrameIndex{0},
                *frameCount.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
        require(!mismatchedSource);
    }

    // Source clocks not authorized by M19 six-rate product contract reject
    // categorically instead of silently substituting a 48-kHz K bank.
    auto unrecognized=makeBuffer(32000,ChannelLayout::MONO_C,0,left);
    require(unrecognized.has_value());
    auto fourFrames=rgsml::core::FrameCount::create(4);
    require(fourFrames.has_value());
    auto invalidRate=rgsml::analysis::internal::m19::
        measure_selected_scope_candidate(
            unrecognized.value()->view(),rgsml::core::FrameIndex{0},
            *fourFrames.value(),FrameDomainId::SOURCE_PROCESSING_RATE);
    require(!invalidRate);
    require(invalidRate.error()->code()==
            rgsml::core::ErrorCode::UnsupportedOperation);

    // Last M19 premerge integration proof with ACTUAL unchanged canonical
    // AudioBufferView/subview, not a copied synthetic span or substitute M16
    // region detector. Cover all PO-frozen clocks, nonzero selected origin,
    // two complete overlapping K blocks and poisoned OUTSIDE PCM. Reuse the
    // same source-owned K authority and never expose unqualified M16 TP.
    for (const std::int64_t rate : {
            44100,48000,88200,96000,176400,192000}) {
        const std::size_t begin=31U;
        const std::size_t window=static_cast<std::size_t>(rate*2/5);
        const std::size_t hop=static_cast<std::size_t>(rate/10);
        const std::size_t count=window+hop;
        std::vector<double> actual(begin+count+17U,1000.0);
        std::fill(actual.begin()+begin,
                  actual.begin()+begin+count,0.125);
        actual[begin-1U]=(std::numeric_limits<double>::quiet_NaN)();
        actual[begin+count]=(std::numeric_limits<double>::infinity)();
        const auto domain=(rate % 3==0) ? FrameDomainId::OUTPUT_RATE
                                         : FrameDomainId::SOURCE_PROCESSING_RATE;
        auto owner=makeBuffer(rate,ChannelLayout::MONO_C,0,actual,{},domain);
        require(owner.has_value());
        const auto frames=rgsml::core::FrameCount::create(
            static_cast<std::int64_t>(count));
        require(frames.has_value());
        const auto scope=rgsml::core::FrameIndex{
            static_cast<std::int64_t>(begin)};
        const auto selected=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),scope,*frames.value(),domain);
        require(selected.has_value());
        require(selected.value()->integrated_lufs.has_value());
        require(selected.value()->loudness_state ==
                rgsml::analysis::internal::m19::
                    LoudnessPreproductionState::available);
        require(std::abs(*selected.value()->integrated_lufs -
                          (-26.2768703195911+20.0*std::log10(0.125)))
                < 0.00005);
        require(selected.value()->sample_rate_hz ==
                static_cast<std::uint32_t>(rate));
        require(selected.value()->scope_begin_frame==begin);
        require(selected.value()->scope_frames==count);
        require(selected.value()->full_scope_rms_dbfs.has_value());
        require(selected.value()->sample_peak_dbfs.has_value());
        require(selected.value()->sample_peak_crest_db.has_value());
        require(!selected.value()->true_peak_db_tp);
        const auto* bank = rgsml::analysis::internal::m19::
            find_frozen_k_weighting_bank_candidate(
                static_cast<std::uint32_t>(rate));
        require(bank!=nullptr);
        require(selected.value()->k_weighting_bank_member_id==bank->member_id);
        require(selected.value()->k_weighting_bank_sha256==
                bank->canonical_le_sha256);

        auto repeat=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),scope,*frames.value(),domain);
        require(repeat.has_value());
        require(selected.value()->integrated_lufs==repeat.value()->integrated_lufs);
        require(selected.value()->full_scope_rms_dbfs==
                repeat.value()->full_scope_rms_dbfs);

        const auto original=owner.value()->view().channel(0U);
        require(original.has_value());
        require(std::isnan((*original.value())[begin-1U]));
        require(std::isinf((*original.value())[begin+count]));
        const auto forbiddenTp=measure_true_peak_full_resource(*owner.value());
        require(!forbiddenTp);
        require(forbiddenTp.error()->code() ==
                rgsml::core::ErrorCode::InvalidAudioSample);

        const auto oppositeDomain=(domain==FrameDomainId::OUTPUT_RATE)
                ?FrameDomainId::SOURCE_PROCESSING_RATE
                :FrameDomainId::OUTPUT_RATE;
        const auto wrong=rgsml::analysis::internal::m19::
            measure_selected_scope_candidate(
                owner.value()->view(),scope,*frames.value(),oppositeDomain);
        require(!wrong);
        require(wrong.error()->code()==
                rgsml::core::ErrorCode::InvalidFrameRange);
    }
    return 0;
}
