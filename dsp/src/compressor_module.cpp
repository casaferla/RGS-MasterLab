#include <rgsml/dsp/compressor_module.hpp>

#include <rgsml/audio/audio_format.hpp>
#include <rgsml/core/checked_integer.hpp>
#include <rgsml/core/error.hpp>
#include <rgsml/dsp/module_descriptor.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::dsp {
namespace {

constexpr auto kCompressorTypeId = "rgsml.dsp.compressor";
constexpr auto kCompressorAlgorithmVersion = "1.0.0";
constexpr auto kCompressorParameterSchema = "rgsml.dsp.compressor.parameters/1.0.0";

[[nodiscard]] rgsml::core::Error comp_error(
    rgsml::core::ErrorCode code,
    std::string category,
    std::string message)
{
    return rgsml::core::Error{
        code, std::move(message), {{"category", std::move(category)}}};
}

[[nodiscard]] std::int64_t round_ties_to_even(double x) noexcept
{
    const double floor_val = std::floor(x);
    const double diff = x - floor_val;
    if (diff < 0.5) {
        return static_cast<std::int64_t>(floor_val);
    }
    if (diff > 0.5) {
        return static_cast<std::int64_t>(floor_val + 1.0);
    }
    const std::int64_t int_floor = static_cast<std::int64_t>(floor_val);
    return (int_floor % 2 == 0) ? int_floor : (int_floor + 1);
}

[[nodiscard]] bool valid_domain(rgsml::audio::FrameDomainId domain) noexcept
{
    return domain == rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE
        || domain == rgsml::audio::FrameDomainId::OUTPUT_RATE;
}

[[nodiscard]] rgsml::core::Status validate_spec(const DspProcessSpec& spec)
{
    if (!valid_domain(spec.frame_domain_id)
        || spec.maximum_block_frames.value() <= 0
        || (spec.audio_format.channel_layout() != rgsml::audio::ChannelLayout::MONO_C
            && spec.audio_format.channel_layout() != rgsml::audio::ChannelLayout::STEREO_LR)) {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INVALID_DSP_PROCESS_SPEC",
            "Compressor requires binary64 planar mono/stereo process specification."));
    }
    return rgsml::core::Status::success();
}

[[nodiscard]] std::string encode_double_hex(double val) noexcept
{
    if (val == 0.0) {
        val = 0.0;
    }
    const auto bits = std::bit_cast<std::uint64_t>(val);
    constexpr char hex_digits[] = "0123456789abcdef";
    std::string out(16, '0');
    for (std::size_t i = 0; i < 16; ++i) {
        const auto shift = (15 - i) * 4;
        out[i] = hex_digits[(bits >> shift) & 0x0F];
    }
    return out;
}

[[nodiscard]] std::string compute_sonic_fingerprint(
    const CompressorParameters& params,
    bool is_mono)
{
    std::string fp = "rgsml.dsp.compressor.sonic/1.0.0;det=";
    fp += (params.detector_mode() == CompressorDetectorMode::PEAK ? "PEAK" : "RMS");
    fp += ";link=";
    if (is_mono) {
        fp += "NONE";
    } else {
        switch (params.channel_link()) {
        case CompressorChannelLink::LINKED_MAX: fp += "LINKED_MAX"; break;
        case CompressorChannelLink::LINKED_MEAN: fp += "LINKED_MEAN"; break;
        case CompressorChannelLink::DUAL_MONO: fp += "DUAL_MONO"; break;
        }
    }
    fp += ";thresh=" + encode_double_hex(params.threshold_dbfs());
    fp += ";rat=" + encode_double_hex(params.ratio());
    fp += ";knee=" + encode_double_hex(params.knee_db());
    fp += ";att=" + encode_double_hex(params.attack_ms());
    fp += ";rel=" + encode_double_hex(params.release_ms());
    fp += ";rms=" + encode_double_hex(params.rms_time_constant_ms());
    fp += ";look=" + encode_double_hex(params.look_ahead_ms());
    fp += ";mix=" + encode_double_hex(params.mix_percent());
    fp += ";make=" + encode_double_hex(params.makeup_gain_db());
    return fp;
}

}  // namespace

struct CompressorModule::Impl final {
    const ModuleDescriptor* descriptor{nullptr};
    CompressorParameters parameters{*CompressorParameters::create_default().value()};
    std::optional<DspProcessSpec> prepared_spec;

    std::int64_t lookahead_frames{0};
    double a_rms{0.0};
    double a_attack{0.0};
    double a_release{0.0};

    bool bound{false};
    std::optional<rgsml::core::FrameIndex> next_input_frame;
    bool in_finalize{false};
    bool finalized{false};
    std::int64_t tail_emitted{0};

    std::vector<double> rms_states;
    std::vector<double> smoothed_reduction_db;
    std::vector<std::vector<double>> delay_buffers;
    std::vector<std::size_t> delay_cursors;

    void update_coefficients(double sample_rate) noexcept
    {
        lookahead_frames = round_ties_to_even(parameters.look_ahead_ms() * sample_rate / 1000.0);

        const double rms_sec = parameters.rms_time_constant_ms() / 1000.0;
        a_rms = std::exp(-1.0 / (rms_sec * sample_rate));

        const double att_sec = parameters.attack_ms() / 1000.0;
        a_attack = std::exp(-1.0 / (att_sec * sample_rate));

        const double rel_sec = parameters.release_ms() / 1000.0;
        a_release = std::exp(-1.0 / (rel_sec * sample_rate));
    }
};

rgsml::core::Result<std::unique_ptr<CompressorModule>> CompressorModule::create(
    const ModuleDescriptor& descriptor,
    CompressorParameters parameters)
{
    const auto algorithm = descriptor.algorithm_version();
    const auto schema = descriptor.parameter_schema_id();
    if (descriptor.type_id() != kCompressorTypeId
        || !algorithm || *algorithm != kCompressorAlgorithmVersion
        || !schema || *schema != kCompressorParameterSchema) {
        return rgsml::core::Result<std::unique_ptr<CompressorModule>>::failure(comp_error(
            rgsml::core::ErrorCode::UnsupportedOperation,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "The descriptor does not identify the frozen Compressor-v1 implementation."));
    }

    try {
        auto impl = std::make_unique<Impl>();
        impl->descriptor = &descriptor;
        impl->parameters = parameters;
        return rgsml::core::Result<std::unique_ptr<CompressorModule>>::success(
            std::unique_ptr<CompressorModule>{new CompressorModule{std::move(impl)}});
    } catch (...) {
        return rgsml::core::Result<std::unique_ptr<CompressorModule>>::failure(comp_error(
            rgsml::core::ErrorCode::InvalidState,
            "MODULE_IMPLEMENTATION_UNAVAILABLE",
            "Compressor-v1 construction failed."));
    }
}

CompressorModule::CompressorModule(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

CompressorModule::~CompressorModule() = default;

const CompressorParameters& CompressorModule::parameters() const noexcept
{
    return impl_->parameters;
}

const ModuleDescriptor& CompressorModule::descriptor() const noexcept
{
    return *impl_->descriptor;
}

rgsml::core::Result<DspRuntimeRequirements> CompressorModule::runtime_requirements(
    const DspProcessSpec& spec) const
{
    const auto valid = validate_spec(spec);
    if (!valid) {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(*valid.error());
    }

    const double sr = static_cast<double>(spec.audio_format.sample_rate().value());
    const std::int64_t look_frames = round_ties_to_even(impl_->parameters.look_ahead_ms() * sr / 1000.0);

    const double rms_sec = impl_->parameters.rms_time_constant_ms() / 1000.0;
    const double a_rms_val = std::exp(-1.0 / (rms_sec * sr));

    const double att_sec = impl_->parameters.attack_ms() / 1000.0;
    const double a_att_val = std::exp(-1.0 / (att_sec * sr));

    const double rel_sec = impl_->parameters.release_ms() / 1000.0;
    const double a_rel_val = std::exp(-1.0 / (rel_sec * sr));

    const auto calc_n = [](double a) -> std::int64_t {
        if (a <= 0.0 || a >= 1.0) return 0;
        const double log_a = std::log(a);
        if (log_a == 0.0) return 0;
        return static_cast<std::int64_t>(std::ceil(-13.815510557964274 / log_a));
    };

    const std::int64_t n_rms = (impl_->parameters.detector_mode() == CompressorDetectorMode::RMS) ? calc_n(a_rms_val) : 0;
    const std::int64_t n_gain = std::max(calc_n(a_att_val), calc_n(a_rel_val));

    const auto settling_res = rgsml::core::checked_add(n_rms, n_gain);
    if (!settling_res) {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(*settling_res.error());
    }
    const std::int64_t settling = *settling_res.value();

    const auto pre_res = rgsml::core::checked_add(look_frames, settling);
    if (!pre_res) {
        return rgsml::core::Result<DspRuntimeRequirements>::failure(*pre_res.error());
    }

    const auto lat = *rgsml::core::FrameCount::create(look_frames).value();
    const auto pre_cnt = *rgsml::core::FrameCount::create(*pre_res.value()).value();

    return rgsml::core::Result<DspRuntimeRequirements>::success(
        DspRuntimeRequirements{
            DspExecutionModel::STREAMING_CAUSAL,
            lat,
            lat,
            pre_cnt,
            lat,
            lat,
            false});
}

rgsml::core::Status CompressorModule::prepare(const DspProcessSpec& spec)
{
    const auto valid = validate_spec(spec);
    if (!valid) {
        return valid;
    }
    impl_->prepared_spec = spec;
    const double sr = static_cast<double>(spec.audio_format.sample_rate().value());
    impl_->update_coefficients(sr);

    const std::size_t channels = spec.audio_format.channel_count();
    const std::size_t delay_cap = (impl_->lookahead_frames > 0) ? static_cast<std::size_t>(impl_->lookahead_frames) : 1U;

    impl_->rms_states.assign(channels, 0.0);
    impl_->smoothed_reduction_db.assign(channels, 0.0);
    impl_->delay_cursors.assign(channels, 0U);
    impl_->delay_buffers.assign(channels, std::vector<double>(delay_cap, 0.0));

    reset();
    return rgsml::core::Status::success();
}

void CompressorModule::reset() noexcept
{
    impl_->bound = false;
    impl_->next_input_frame = std::nullopt;
    impl_->in_finalize = false;
    impl_->finalized = false;
    impl_->tail_emitted = 0;

    std::fill(impl_->rms_states.begin(), impl_->rms_states.end(), 0.0);
    std::fill(impl_->smoothed_reduction_db.begin(), impl_->smoothed_reduction_db.end(), 0.0);
    std::fill(impl_->delay_cursors.begin(), impl_->delay_cursors.end(), 0U);
    for (auto& buf : impl_->delay_buffers) {
        std::fill(buf.begin(), buf.end(), 0.0);
    }
}

rgsml::core::Status CompressorModule::process(
    rgsml::audio::AudioBufferView input,
    rgsml::audio::MutableAudioBufferView output,
    const DspProcessContext& context)
{
    try {
        if (!impl_->prepared_spec) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidState,
                "DSP_MODULE_NOT_PREPARED",
                "Compressor must be prepared before processing."));
        }
        if (impl_->in_finalize || impl_->finalized) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidState,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Cannot call process after finalize."));
        }

        if (input.format() != impl_->prepared_spec->audio_format
            || output.format() != impl_->prepared_spec->audio_format) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Process input/output audio format mismatch."));
        }

        if (input.timebase().frame_domain_id() != impl_->prepared_spec->frame_domain_id
            || output.timebase().frame_domain_id() != impl_->prepared_spec->frame_domain_id) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Process frame domain mismatch."));
        }

        if (input.frame_count() != output.frame_count()) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Process input/output frame count mismatch."));
        }

        const auto range_len = context.output_frame_range.length();
        if (!range_len || range_len.value()->value() != input.frame_count().value()) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Process output frame range length mismatch."));
        }

        if (input.frame_count().value() > impl_->prepared_spec->maximum_block_frames.value()) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Process frame count exceeds maximum_block_frames."));
        }

        if (!impl_->bound) {
            if (!context.begins_stream) {
                return rgsml::core::Status::failure(comp_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INVALID_DSP_PROCESS_CONTEXT",
                    "First process call must represent begins_stream = true."));
            }
        } else {
            if (context.begins_stream) {
                return rgsml::core::Status::failure(comp_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INVALID_DSP_PROCESS_CONTEXT",
                    "Repeated begins_stream after binding invalid."));
            }
            if (context.output_frame_range.begin() != *impl_->next_input_frame) {
                return rgsml::core::Status::failure(comp_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INVALID_DSP_PROCESS_CONTEXT",
                    "Process frame range must be contiguous."));
            }
        }

        const auto channels = input.format().channel_count();
        const auto frames = input.frame_count().value();
        const bool is_mono = (channels == 1U);

        // Pre-scan all samples for non-finite values before state mutation
        for (std::size_t ch = 0; ch < channels; ++ch) {
            const auto plane = *input.channel(ch).value();
            for (const double sample : plane) {
                if (!std::isfinite(sample)) {
                    return rgsml::core::Status::failure(comp_error(
                        rgsml::core::ErrorCode::InvalidAudioSample,
                        "INVALID_AUDIO_SAMPLE",
                        "Non-finite audio sample in process input."));
                }
            }
        }

        impl_->bound = true;

        const double threshold = impl_->parameters.threshold_dbfs();
        const double ratio = impl_->parameters.ratio();
        const double knee = impl_->parameters.knee_db();
        const double makeup_factor = std::pow(10.0, impl_->parameters.makeup_gain_db() / 20.0);
        const double mix_m = impl_->parameters.mix_percent() / 100.0;
        const double inv_ratio_sub_one = (1.0 / ratio) - 1.0;
        const double one_sub_inv_ratio = 1.0 - (1.0 / ratio);

        for (std::size_t i = 0; i < static_cast<std::size_t>(frames); ++i) {
            // Step 1: Detect
            double p0 = 0.0;
            double p1 = 0.0;

            if (impl_->parameters.detector_mode() == CompressorDetectorMode::PEAK) {
                p0 = std::abs((*input.channel(0).value())[i]);
                if (!is_mono) {
                    p1 = std::abs((*input.channel(1).value())[i]);
                }
            } else { // RMS
                const double x0 = (*input.channel(0).value())[i];
                impl_->rms_states[0] = impl_->a_rms * impl_->rms_states[0] + (1.0 - impl_->a_rms) * (x0 * x0);
                p0 = std::sqrt(std::max(impl_->rms_states[0], 1e-30));

                if (!is_mono) {
                    const double x1 = (*input.channel(1).value())[i];
                    impl_->rms_states[1] = impl_->a_rms * impl_->rms_states[1] + (1.0 - impl_->a_rms) * (x1 * x1);
                    p1 = std::sqrt(std::max(impl_->rms_states[1], 1e-30));
                }
            }

            // Step 2: Stereo Link
            double pl0 = p0;
            double pl1 = p1;
            if (!is_mono) {
                switch (impl_->parameters.channel_link()) {
                case CompressorChannelLink::LINKED_MAX: {
                    const double mx = std::max(p0, p1);
                    pl0 = mx;
                    pl1 = mx;
                    break;
                }
                case CompressorChannelLink::LINKED_MEAN: {
                    const double mn = std::sqrt((p0 * p0 + p1 * p1) * 0.5);
                    pl0 = mn;
                    pl1 = mn;
                    break;
                }
                case CompressorChannelLink::DUAL_MONO:
                    pl0 = p0;
                    pl1 = p1;
                    break;
                }
            }

            // Step 3: Compute target reduction dB
            const auto compute_target_db = [&](double p) -> double {
                if (p == 0.0 || ratio == 1.0) {
                    return 0.0;
                }
                const double x_db = 20.0 * std::log10(p);
                if (knee == 0.0) {
                    if (x_db <= threshold) return 0.0;
                    return (x_db - threshold) * one_sub_inv_ratio;
                }
                // Soft knee
                const double diff = x_db - threshold;
                if (2.0 * diff < -knee) return 0.0;
                if (2.0 * std::abs(diff) <= knee) {
                    const double term = diff + (knee * 0.5);
                    const double y_db = x_db + (inv_ratio_sub_one * term * term) / (2.0 * knee);
                    return x_db - y_db;
                }
                return diff * one_sub_inv_ratio;
            };

            const double targ0 = compute_target_db(pl0);
            const double targ1 = is_mono ? 0.0 : compute_target_db(pl1);

            // Step 4: Ballistics
            const auto smooth_reduction = [&](double targ, std::size_t ch) -> double {
                const double prev = impl_->smoothed_reduction_db[ch];
                const double coeff = (targ > prev) ? impl_->a_attack : impl_->a_release;
                const double curr = coeff * prev + (1.0 - coeff) * targ;
                impl_->smoothed_reduction_db[ch] = curr;
                return curr;
            };

            const double red0 = smooth_reduction(targ0, 0);
            const double red1 = is_mono ? 0.0 : smooth_reduction(targ1, 1);

            // Step 5: Delay + Mix + Makeup
            for (std::size_t ch = 0; ch < channels; ++ch) {
                const double in_sample = (*input.channel(ch).value())[i];
                const double red = (ch == 0) ? red0 : red1;

                double x_delayed = in_sample;
                if (impl_->lookahead_frames > 0) {
                    auto& buf = impl_->delay_buffers[ch];
                    auto& cursor = impl_->delay_cursors[ch];
                    x_delayed = buf[cursor];
                    buf[cursor] = in_sample;
                    cursor = (cursor + 1U) % static_cast<std::size_t>(impl_->lookahead_frames);
                }

                const double gain_lin = std::pow(10.0, -red / 20.0);
                const double wet = x_delayed * gain_lin * makeup_factor;
                const double out_sample = (1.0 - mix_m) * x_delayed + mix_m * wet;

                (*output.channel(ch).value())[i] = out_sample;
            }
        }

        impl_->next_input_frame = context.output_frame_range.end();
        return rgsml::core::Status::success();
    } catch (...) {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_PROCESS_FAILURE",
            "Compressor process contained an internal failure."));
    }
}

rgsml::core::Status CompressorModule::finalize(
    rgsml::audio::MutableAudioBufferView output,
    const DspProcessContext& context)
{
    try {
        if (!impl_->prepared_spec) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidState,
                "DSP_MODULE_NOT_PREPARED",
                "Compressor must be prepared before finalize."));
        }

        if (impl_->finalized) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidState,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Cannot call finalize after completed finalize."));
        }

        if (output.format() != impl_->prepared_spec->audio_format) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Finalize output audio format mismatch."));
        }

        if (output.timebase().frame_domain_id() != impl_->prepared_spec->frame_domain_id) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Finalize frame domain mismatch."));
        }

        const auto range_len = context.output_frame_range.length();
        if (!range_len || range_len.value()->value() != output.frame_count().value()) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Finalize output frame range length mismatch."));
        }

        if (output.frame_count().value() > impl_->prepared_spec->maximum_block_frames.value()) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Finalize frame count exceeds maximum_block_frames."));
        }

        if (!impl_->bound) {
            if (!context.begins_stream) {
                return rgsml::core::Status::failure(comp_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INVALID_DSP_PROCESS_CONTEXT",
                    "First empty-stream finalize call must represent begins_stream = true."));
            }
        } else {
            if (context.begins_stream) {
                return rgsml::core::Status::failure(comp_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INVALID_DSP_PROCESS_CONTEXT",
                    "Repeated begins_stream after binding invalid."));
            }
            if (context.output_frame_range.begin() != *impl_->next_input_frame) {
                return rgsml::core::Status::failure(comp_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INVALID_DSP_PROCESS_CONTEXT",
                    "Finalize frame range must be contiguous."));
            }
        }

        const std::int64_t remaining_drain = impl_->lookahead_frames - impl_->tail_emitted;
        const std::int64_t req_frames = output.frame_count().value();
        if (req_frames > remaining_drain) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INVALID_DSP_PROCESS_CONTEXT",
                "Finalize requested frame count exceeds remaining drain."));
        }

        impl_->in_finalize = true;
        impl_->bound = true;

        const auto channels = output.format().channel_count();
        const auto frames = output.frame_count().value();
        const bool is_mono = (channels == 1U);

        const double threshold = impl_->parameters.threshold_dbfs();
        const double ratio = impl_->parameters.ratio();
        const double knee = impl_->parameters.knee_db();
        const double makeup_factor = std::pow(10.0, impl_->parameters.makeup_gain_db() / 20.0);
        const double mix_m = impl_->parameters.mix_percent() / 100.0;
        const double inv_ratio_sub_one = (1.0 / ratio) - 1.0;
        const double one_sub_inv_ratio = 1.0 - (1.0 / ratio);

        const auto compute_target_db = [&](double p) -> double {
            if (p == 0.0 || ratio == 1.0) {
                return 0.0;
            }
            const double x_db = 20.0 * std::log10(p);
            if (knee == 0.0) {
                if (x_db <= threshold) return 0.0;
                return (x_db - threshold) * one_sub_inv_ratio;
            }
            // Soft knee
            const double diff = x_db - threshold;
            if (2.0 * diff < -knee) return 0.0;
            if (2.0 * std::abs(diff) <= knee) {
                const double term = diff + (knee * 0.5);
                const double y_db = x_db + (inv_ratio_sub_one * term * term) / (2.0 * knee);
                return x_db - y_db;
            }
            return diff * one_sub_inv_ratio;
        };

        const auto smooth_reduction = [&](double targ, std::size_t ch) -> double {
            const double prev = impl_->smoothed_reduction_db[ch];
            const double coeff = (targ > prev) ? impl_->a_attack : impl_->a_release;
            const double curr = coeff * prev + (1.0 - coeff) * targ;
            impl_->smoothed_reduction_db[ch] = curr;
            return curr;
        };

        for (std::size_t i = 0; i < static_cast<std::size_t>(frames); ++i) {
            double p0 = 0.0;
            double p1 = 0.0;

            if (impl_->parameters.detector_mode() == CompressorDetectorMode::PEAK) {
                p0 = 0.0;
                p1 = 0.0;
            } else { // RMS recurrence with +0.0 input
                impl_->rms_states[0] = impl_->a_rms * impl_->rms_states[0] + (1.0 - impl_->a_rms) * 0.0;
                p0 = std::sqrt(std::max(impl_->rms_states[0], 1e-30));

                if (!is_mono) {
                    impl_->rms_states[1] = impl_->a_rms * impl_->rms_states[1] + (1.0 - impl_->a_rms) * 0.0;
                    p1 = std::sqrt(std::max(impl_->rms_states[1], 1e-30));
                }
            }

            // Stereo link
            double pl0 = p0;
            double pl1 = p1;
            if (!is_mono) {
                switch (impl_->parameters.channel_link()) {
                case CompressorChannelLink::LINKED_MAX: {
                    const double mx = std::max(p0, p1);
                    pl0 = mx;
                    pl1 = mx;
                    break;
                }
                case CompressorChannelLink::LINKED_MEAN: {
                    const double mn = std::sqrt((p0 * p0 + p1 * p1) * 0.5);
                    pl0 = mn;
                    pl1 = mn;
                    break;
                }
                case CompressorChannelLink::DUAL_MONO:
                    pl0 = p0;
                    pl1 = p1;
                    break;
                }
            }

            const double targ0 = compute_target_db(pl0);
            const double targ1 = is_mono ? 0.0 : compute_target_db(pl1);

            const double red0 = smooth_reduction(targ0, 0);
            const double red1 = is_mono ? 0.0 : smooth_reduction(targ1, 1);

            for (std::size_t ch = 0; ch < channels; ++ch) {
                const double red = (ch == 0) ? red0 : red1;

                double x_delayed = 0.0;
                if (impl_->lookahead_frames > 0) {
                    auto& buf = impl_->delay_buffers[ch];
                    auto& cursor = impl_->delay_cursors[ch];
                    x_delayed = buf[cursor];
                    buf[cursor] = 0.0;
                    cursor = (cursor + 1U) % static_cast<std::size_t>(impl_->lookahead_frames);
                }

                const double gain_lin = std::pow(10.0, -red / 20.0);
                const double wet = x_delayed * gain_lin * makeup_factor;
                const double out_sample = (1.0 - mix_m) * x_delayed + mix_m * wet;

                (*output.channel(ch).value())[i] = out_sample;
            }
        }

        impl_->tail_emitted += frames;
        if (impl_->tail_emitted >= impl_->lookahead_frames) {
            impl_->finalized = true;
        }

        impl_->next_input_frame = context.output_frame_range.end();
        return rgsml::core::Status::success();
    } catch (...) {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_PROCESS_FAILURE",
            "Compressor finalize contained an internal failure."));
    }
}

rgsml::core::Result<DspRuntimeCheckpoint> CompressorModule::runtime_checkpoint() const
{
    if (!impl_->prepared_spec) {
        return rgsml::core::Result<DspRuntimeCheckpoint>::failure(comp_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_MODULE_NOT_PREPARED",
            "Compressor must be prepared before checkpoint creation."));
    }

    if (impl_->in_finalize || impl_->finalized) {
        return rgsml::core::Result<DspRuntimeCheckpoint>::failure(comp_error(
            rgsml::core::ErrorCode::UnsupportedOperation,
            "RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED",
            "Checkpoint during or after finalize rejected."));
    }

    try {
        const bool is_mono = (impl_->prepared_spec->audio_format.channel_layout() == rgsml::audio::ChannelLayout::MONO_C);
        DspRuntimeCheckpoint cp;
        cp.module_type_id = kCompressorTypeId;
        cp.algorithm_version = kCompressorAlgorithmVersion;
        cp.parameter_schema_id = kCompressorParameterSchema;
        cp.sonic_fingerprint = compute_sonic_fingerprint(impl_->parameters, is_mono);
        cp.audio_format = impl_->prepared_spec->audio_format;
        cp.frame_domain_id = impl_->prepared_spec->frame_domain_id;
        cp.checkpoint_schema_version = "rgsml.dsp.compressor.checkpoint/1.0.0";
        cp.next_input_frame = impl_->next_input_frame;
        cp.backend_identity = "rgsml.dsp.backend.default.v1";

        const auto write_double = [&cp](double val) {
            const auto bits_val = std::bit_cast<std::uint64_t>(val);
            for (std::size_t b = 0; b < 8; ++b) {
                cp.payload.push_back(static_cast<std::uint8_t>((bits_val >> (b * 8)) & 0xFF));
            }
        };

        const auto write_u64 = [&cp](std::uint64_t val) {
            for (std::size_t b = 0; b < 8; ++b) {
                cp.payload.push_back(static_cast<std::uint8_t>((val >> (b * 8)) & 0xFF));
            }
        };

        const std::size_t channels = impl_->rms_states.size();
        write_u64(channels);
        for (std::size_t ch = 0; ch < channels; ++ch) {
            write_double(impl_->rms_states[ch]);
            write_double(impl_->smoothed_reduction_db[ch]);
            write_u64(static_cast<std::uint64_t>(impl_->delay_cursors[ch]));

            const auto& buf = impl_->delay_buffers[ch];
            write_u64(buf.size());
            for (const double sample : buf) {
                write_double(sample);
            }
        }

        return rgsml::core::Result<DspRuntimeCheckpoint>::success(std::move(cp));
    } catch (...) {
        return rgsml::core::Result<DspRuntimeCheckpoint>::failure(comp_error(
            rgsml::core::ErrorCode::InvalidState,
            "CHECKPOINT_CREATION_FAILURE",
            "Compressor checkpoint creation failed."));
    }
}

rgsml::core::Status CompressorModule::restore_runtime_checkpoint(
    const DspRuntimeCheckpoint& checkpoint)
{
    if (!impl_->prepared_spec) {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidState,
            "DSP_MODULE_NOT_PREPARED",
            "Compressor must be prepared before restoring checkpoint."));
    }

    const bool is_mono = (impl_->prepared_spec->audio_format.channel_layout() == rgsml::audio::ChannelLayout::MONO_C);
    const auto expected_fp = compute_sonic_fingerprint(impl_->parameters, is_mono);

    // 1. Atomic Validation
    if (checkpoint.module_type_id != kCompressorTypeId
        || checkpoint.algorithm_version != kCompressorAlgorithmVersion
        || checkpoint.parameter_schema_id != kCompressorParameterSchema
        || checkpoint.sonic_fingerprint != expected_fp
        || checkpoint.audio_format != impl_->prepared_spec->audio_format
        || checkpoint.frame_domain_id != impl_->prepared_spec->frame_domain_id
        || checkpoint.checkpoint_schema_version != "rgsml.dsp.compressor.checkpoint/1.0.0"
        || checkpoint.backend_identity != "rgsml.dsp.backend.default.v1") {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INCOMPATIBLE_CHECKPOINT",
            "Compressor checkpoint metadata mismatch."));
    }

    if (impl_->bound && checkpoint.next_input_frame != impl_->next_input_frame) {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INCOMPATIBLE_CHECKPOINT",
            "Bound module cannot restore checkpoint with non-matching next_input_frame."));
    }

    // Parse payload
    const auto read_u64 = [&checkpoint](std::size_t& offset) -> std::optional<std::uint64_t> {
        if (offset + 8 > checkpoint.payload.size()) return std::nullopt;
        std::uint64_t val = 0;
        for (std::size_t b = 0; b < 8; ++b) {
            val |= static_cast<std::uint64_t>(checkpoint.payload[offset + b]) << (b * 8);
        }
        offset += 8;
        return val;
    };

    const auto read_double = [&checkpoint](std::size_t& offset) -> std::optional<double> {
        if (offset + 8 > checkpoint.payload.size()) return std::nullopt;
        std::uint64_t bits_val = 0;
        for (std::size_t b = 0; b < 8; ++b) {
            bits_val |= static_cast<std::uint64_t>(checkpoint.payload[offset + b]) << (b * 8);
        }
        offset += 8;
        return std::bit_cast<double>(bits_val);
    };

    std::size_t offset = 0;
    const auto ch_cnt_opt = read_u64(offset);
    if (!ch_cnt_opt || *ch_cnt_opt != impl_->prepared_spec->audio_format.channel_count()) {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INCOMPATIBLE_CHECKPOINT",
            "Compressor payload channel count mismatch."));
    }

    const std::size_t channels = static_cast<std::size_t>(*ch_cnt_opt);
    const std::size_t expected_delay_cap = (impl_->lookahead_frames > 0) ? static_cast<std::size_t>(impl_->lookahead_frames) : 1U;

    std::vector<double> new_rms(channels);
    std::vector<double> new_smoothed(channels);
    std::vector<std::size_t> new_cursors(channels);
    std::vector<std::vector<double>> new_buffers(channels);

    for (std::size_t ch = 0; ch < channels; ++ch) {
        const auto rms_opt = read_double(offset);
        const auto sm_opt = read_double(offset);
        const auto cur_opt = read_u64(offset);
        const auto buf_sz_opt = read_u64(offset);

        if (!rms_opt || !sm_opt || !cur_opt || !buf_sz_opt) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INCOMPATIBLE_CHECKPOINT",
                "Compressor payload truncated."));
        }

        if (!std::isfinite(*rms_opt) || *rms_opt < 0.0 || !std::isfinite(*sm_opt)) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INCOMPATIBLE_CHECKPOINT",
                "Compressor payload contain non-finite or invalid state values."));
        }

        if (*cur_opt >= expected_delay_cap) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INCOMPATIBLE_CHECKPOINT",
                "Compressor payload cursor out of bounds."));
        }

        if (*buf_sz_opt != expected_delay_cap) {
            return rgsml::core::Status::failure(comp_error(
                rgsml::core::ErrorCode::InvalidArgument,
                "INCOMPATIBLE_CHECKPOINT",
                "Compressor payload delay buffer length mismatch."));
        }

        new_rms[ch] = *rms_opt;
        new_smoothed[ch] = *sm_opt;
        new_cursors[ch] = static_cast<std::size_t>(*cur_opt);

        new_buffers[ch].reserve(expected_delay_cap);
        for (std::size_t s = 0; s < expected_delay_cap; ++s) {
            const auto samp_opt = read_double(offset);
            if (!samp_opt || !std::isfinite(*samp_opt)) {
                return rgsml::core::Status::failure(comp_error(
                    rgsml::core::ErrorCode::InvalidArgument,
                    "INCOMPATIBLE_CHECKPOINT",
                    "Compressor payload buffer sample non-finite or truncated."));
            }
            new_buffers[ch].push_back(*samp_opt);
        }
    }

    if (offset != checkpoint.payload.size()) {
        return rgsml::core::Status::failure(comp_error(
            rgsml::core::ErrorCode::InvalidArgument,
            "INCOMPATIBLE_CHECKPOINT",
            "Compressor payload trailing bytes rejected."));
    }

    // 2. Commit restored state atomically
    impl_->next_input_frame = checkpoint.next_input_frame;
    impl_->bound = checkpoint.next_input_frame.has_value();
    impl_->in_finalize = false;
    impl_->finalized = false;
    impl_->tail_emitted = 0;

    impl_->rms_states = std::move(new_rms);
    impl_->smoothed_reduction_db = std::move(new_smoothed);
    impl_->delay_cursors = std::move(new_cursors);
    impl_->delay_buffers = std::move(new_buffers);

    return rgsml::core::Status::success();
}

}  // namespace rgsml::dsp
