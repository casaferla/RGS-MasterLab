// M19 preproduction dependency proof against the *existing* public M16 API.
// This does not claim M16 is qualified or provide an M19 region-TP adapter.
// No second oversampler, FIR, or alternative true peak estimate exists here.
#include "internal/m19_basic_level_candidate.hpp"
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
    const std::vector<double>& a, const std::vector<double>& b = {})
{
    auto rate = rgsml::core::SampleRate::create(hz);
    if (!rate) return rgsml::core::Result<AudioBuffer>::failure(*rate.error());
    auto format = AudioFormat::create(*rate.value(), layout);
    if (!format) return rgsml::core::Result<AudioBuffer>::failure(*format.error());
    auto frameCount = rgsml::core::FrameCount::create(
        static_cast<std::int64_t>(a.size()));
    if (!frameCount) return rgsml::core::Result<AudioBuffer>::failure(*frameCount.error());
    auto buf = AudioBuffer::create(*format.value(), FrameDomainId::SOURCE_PROCESSING_RATE,
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
    return 0;
}
