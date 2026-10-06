#include "render_test_support.hpp"

#include <rgsml/core/checked_integer.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/dsp_runtime_checkpoint.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/imodule.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/render/compressor_telemetry_collector.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <QtTest/QTest>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {

using namespace render_support;

[[nodiscard]] rgsml::dsp::ProcessingChain empty_chain(
    const rgsml::dsp::ModuleRegistry& registry)
{
    auto chain = rgsml::dsp::ProcessingChain::create(
        registry,
        {rgsml::dsp::ProcessingStage::MASTER, rgsml::dsp::ChainSegment::MANUAL});
    Q_ASSERT(chain);
    return std::move(*chain.value());
}

[[nodiscard]] rgsml::dsp::GainParameters gain(double value)
{
    auto parameters = rgsml::dsp::GainParameters::create(value);
    Q_ASSERT(parameters);
    return *parameters.value();
}

[[nodiscard]] rgsml::dsp::ParametricEqParameters bell_eq(double freq_hz, double gain_db, double q)
{
    const auto band_id = *rgsml::core::Uuid::parse("10000000-0000-0000-0000-000000000001").value();
    auto band = rgsml::dsp::EqBandParameters::create(
        band_id,
        true,
        rgsml::dsp::EqFilterType::BELL,
        rgsml::dsp::EqRouting::STEREO,
        rgsml::dsp::BellPayload{freq_hz, gain_db, q});
    Q_ASSERT(band);
    auto eq_params = rgsml::dsp::ParametricEqParameters::create({*band.value()});
    Q_ASSERT(eq_params);
    return *eq_params.value();
}

class FakeLatencyModule final : public rgsml::dsp::IModule {
public:
    FakeLatencyModule(
        rgsml::dsp::ModuleDescriptor descriptor,
        std::int64_t latency_frames,
        std::string sonic_fingerprint = "fake.sonic.v1",
        std::string backend_identity = "rgsml.dsp.backend.test",
        double gain_factor = 1.0)
        : descriptor_(std::move(descriptor))
        , latency_frames_(latency_frames)
        , sonic_fingerprint_(std::move(sonic_fingerprint))
        , backend_identity_(std::move(backend_identity))
        , gain_factor_(gain_factor)
    {
    }

    const rgsml::dsp::ModuleDescriptor& descriptor() const noexcept override
    {
        return descriptor_;
    }

    rgsml::core::Result<rgsml::dsp::DspRuntimeRequirements>
    runtime_requirements(const rgsml::dsp::DspProcessSpec&) const override
    {
        const auto zero = *rgsml::core::FrameCount::create(0).value();
        const auto lat = *rgsml::core::FrameCount::create(latency_frames_).value();
        return rgsml::core::Result<rgsml::dsp::DspRuntimeRequirements>::success(
            rgsml::dsp::DspRuntimeRequirements{
                rgsml::dsp::DspExecutionModel::STREAMING_CAUSAL,
                lat,
                lat,
                zero,
                zero,
                lat,
                false});
    }

    rgsml::core::Status prepare(const rgsml::dsp::DspProcessSpec& spec) override
    {
        prepared_spec_ = spec;
        reset();
        return rgsml::core::Status::success();
    }

    void reset() noexcept override
    {
        bound_ = false;
        next_input_frame_ = std::nullopt;
        in_finalize_ = false;
        finalized_ = false;
        tail_emitted_ = 0;
        if (prepared_spec_.has_value()) {
            const auto channels = prepared_spec_->audio_format.channel_count();
            delay_buffers_.assign(channels, std::vector<double>(static_cast<std::size_t>(latency_frames_), 0.0));
        }
    }

    rgsml::core::Status process(
        rgsml::audio::AudioBufferView input,
        rgsml::audio::MutableAudioBufferView output,
        const rgsml::dsp::DspProcessContext& context) override
    {
        if (!prepared_spec_) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidState,
                "DSP_MODULE_NOT_PREPARED",
                {{"category", "DSP_MODULE_NOT_PREPARED"}}});
        }
        if (in_finalize_ || finalized_) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidState,
                "Process called after finalize",
                {{"category", "INVALID_DSP_PROCESS_CONTEXT"}}});
        }

        if (!bound_) {
            if (!context.begins_stream) {
                return rgsml::core::Status::failure(rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidArgument,
                    "First process call must have begins_stream = true",
                    {{"category", "INVALID_DSP_PROCESS_CONTEXT"}}});
            }
            bound_ = true;
            next_input_frame_ = context.output_frame_range.begin();
        } else {
            if (context.output_frame_range.begin() != *next_input_frame_) {
                return rgsml::core::Status::failure(rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidArgument,
                    "Process frame range must be contiguous",
                    {{"category", "INVALID_DSP_PROCESS_CONTEXT"}}});
            }
        }

        const auto channel_count = input.format().channel_count();
        const auto frame_count = input.frame_count().value();

        for (std::size_t ch = 0; ch < channel_count; ++ch) {
            const auto in_plane = *input.channel(ch).value();
            auto out_plane = *output.channel(ch).value();
            auto& buffer = delay_buffers_[ch];

            for (std::size_t i = 0; i < static_cast<std::size_t>(frame_count); ++i) {
                double out_sample = 0.0;
                if (latency_frames_ > 0) {
                    out_sample = buffer.front();
                    buffer.erase(buffer.begin());
                    buffer.push_back(in_plane[i] * gain_factor_);
                } else {
                    out_sample = in_plane[i] * gain_factor_;
                }
                out_plane[i] = out_sample;
            }
        }

        next_input_frame_ = context.output_frame_range.end();
        return rgsml::core::Status::success();
    }

    rgsml::core::Status finalize(
        rgsml::audio::MutableAudioBufferView output,
        const rgsml::dsp::DspProcessContext& context) override
    {
        if (!prepared_spec_) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidState,
                "DSP_MODULE_NOT_PREPARED",
                {{"category", "DSP_MODULE_NOT_PREPARED"}}});
        }

        if (!bound_) {
            if (!context.begins_stream) {
                return rgsml::core::Status::failure(rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidArgument,
                    "First empty-stream finalize call must represent begins_stream = true",
                    {{"category", "INVALID_DSP_PROCESS_CONTEXT"}}});
            }
            bound_ = true;
            next_input_frame_ = context.output_frame_range.begin();
        } else {
            if (context.output_frame_range.begin() != *next_input_frame_) {
                return rgsml::core::Status::failure(rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidArgument,
                    "Finalize frame range must be contiguous",
                    {{"category", "INVALID_DSP_PROCESS_CONTEXT"}}});
            }
        }

        in_finalize_ = true;
        const auto channel_count = output.format().channel_count();
        const auto frame_count = output.frame_count().value();

        for (std::size_t ch = 0; ch < channel_count; ++ch) {
            auto out_plane = *output.channel(ch).value();
            auto& buffer = delay_buffers_[ch];

            for (std::size_t i = 0; i < static_cast<std::size_t>(frame_count); ++i) {
                double out_sample = 0.0;
                if (!buffer.empty()) {
                    out_sample = buffer.front();
                    buffer.erase(buffer.begin());
                }
                out_plane[i] = out_sample;
            }
        }

        tail_emitted_ += frame_count;
        if (tail_emitted_ >= latency_frames_) {
            finalized_ = true;
        }

        next_input_frame_ = context.output_frame_range.end();
        return rgsml::core::Status::success();
    }

    rgsml::core::Result<rgsml::dsp::DspRuntimeCheckpoint>
    runtime_checkpoint() const override
    {
        if (!prepared_spec_) {
            return rgsml::core::Result<rgsml::dsp::DspRuntimeCheckpoint>::failure(
                rgsml::core::Error{
                    rgsml::core::ErrorCode::InvalidState,
                    "DSP_MODULE_NOT_PREPARED",
                    {{"category", "DSP_MODULE_NOT_PREPARED"}}});
        }
        if (in_finalize_ || finalized_) {
            return rgsml::core::Result<rgsml::dsp::DspRuntimeCheckpoint>::failure(
                rgsml::core::Error{
                    rgsml::core::ErrorCode::UnsupportedOperation,
                    "Checkpoint during or after finalize rejected",
                    {{"category", "RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED"}}});
        }

        rgsml::dsp::DspRuntimeCheckpoint cp;
        cp.module_type_id = std::string(descriptor_.type_id());
        cp.algorithm_version = std::string(descriptor_.algorithm_version().value_or("1.0.0"));
        cp.parameter_schema_id = std::string(descriptor_.parameter_schema_id().value_or("rgsml.dsp.fake-latency.parameters/1.0.0"));
        cp.sonic_fingerprint = sonic_fingerprint_;
        if (prepared_spec_) {
            cp.audio_format = prepared_spec_->audio_format;
            cp.frame_domain_id = prepared_spec_->frame_domain_id;
        }
        cp.checkpoint_schema_version = "rgsml.dsp.test-checkpoint/1.0.0";
        cp.next_input_frame = next_input_frame_;
        cp.backend_identity = backend_identity_;

        for (const auto& plane : delay_buffers_) {
            for (const double sample : plane) {
                const auto bits_val = std::bit_cast<std::uint64_t>(sample);
                for (std::size_t b = 0; b < 8; ++b) {
                    cp.payload.push_back(static_cast<std::uint8_t>((bits_val >> (b * 8)) & 0xFF));
                }
            }
        }
        return rgsml::core::Result<rgsml::dsp::DspRuntimeCheckpoint>::success(std::move(cp));
    }

    rgsml::core::Status restore_runtime_checkpoint(
        const rgsml::dsp::DspRuntimeCheckpoint& checkpoint) override
    {
        if (!prepared_spec_) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidState,
                "DSP_MODULE_NOT_PREPARED",
                {{"category", "DSP_MODULE_NOT_PREPARED"}}});
        }
        if (checkpoint.module_type_id != descriptor_.type_id()
            || checkpoint.algorithm_version != descriptor_.algorithm_version().value_or("1.0.0")
            || checkpoint.parameter_schema_id != descriptor_.parameter_schema_id().value_or("rgsml.dsp.fake-latency.parameters/1.0.0")
            || checkpoint.sonic_fingerprint != sonic_fingerprint_
            || checkpoint.audio_format != prepared_spec_->audio_format
            || checkpoint.frame_domain_id != prepared_spec_->frame_domain_id
            || checkpoint.checkpoint_schema_version != "rgsml.dsp.test-checkpoint/1.0.0"
            || checkpoint.backend_identity != backend_identity_) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidArgument,
                "Incompatible checkpoint rejected",
                {{"category", "INCOMPATIBLE_CHECKPOINT"}}});
        }

        if (bound_ && checkpoint.next_input_frame != next_input_frame_) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidArgument,
                "Bound module cannot restore checkpoint with non-matching next_input_frame",
                {{"category", "INCOMPATIBLE_CHECKPOINT"}}});
        }

        const auto channels = prepared_spec_->audio_format.channel_count();
        const std::size_t expected_payload_size = channels * static_cast<std::size_t>(latency_frames_) * 8;
        if (checkpoint.payload.size() != expected_payload_size) {
            return rgsml::core::Status::failure(rgsml::core::Error{
                rgsml::core::ErrorCode::InvalidArgument,
                "Incompatible checkpoint payload shape rejected",
                {{"category", "INCOMPATIBLE_CHECKPOINT"}}});
        }

        next_input_frame_ = checkpoint.next_input_frame;
        bound_ = next_input_frame_.has_value();
        in_finalize_ = false;
        finalized_ = false;

        delay_buffers_.assign(channels, std::vector<double>());
        std::size_t offset = 0;
        for (std::size_t ch = 0; ch < channels; ++ch) {
            for (std::size_t i = 0; i < static_cast<std::size_t>(latency_frames_); ++i) {
                std::uint64_t bits_val = 0;
                for (std::size_t b = 0; b < 8; ++b) {
                    bits_val |= static_cast<std::uint64_t>(checkpoint.payload[offset + b]) << (b * 8);
                }
                offset += 8;
                delay_buffers_[ch].push_back(std::bit_cast<double>(bits_val));
            }
        }
        return rgsml::core::Status::success();
    }

private:
    rgsml::dsp::ModuleDescriptor descriptor_;
    std::int64_t latency_frames_;
    std::string sonic_fingerprint_;
    std::string backend_identity_;
    double gain_factor_;
    std::optional<rgsml::dsp::DspProcessSpec> prepared_spec_;
    bool bound_{false};
    std::optional<rgsml::core::FrameIndex> next_input_frame_;
    bool in_finalize_{false};
    bool finalized_{false};
    std::int64_t tail_emitted_{0};
    std::vector<std::vector<double>> delay_buffers_;
};

class FakeModuleFactory final : public rgsml::dsp::IModuleFactory {
public:
    FakeModuleFactory(
        rgsml::dsp::ModuleDescriptor descriptor,
        std::int64_t latency_frames,
        double gain_factor = 1.0)
        : type_id_(std::string(descriptor.type_id()))
        , descriptor_(std::move(descriptor))
        , latency_frames_(latency_frames)
        , gain_factor_(gain_factor)
    {
    }

    std::string_view module_type_id() const noexcept override
    {
        return type_id_;
    }

    rgsml::core::Result<std::unique_ptr<rgsml::dsp::IModule>> create() const override
    {
        return rgsml::core::Result<std::unique_ptr<rgsml::dsp::IModule>>::success(
            std::make_unique<FakeLatencyModule>(descriptor_, latency_frames_, "fake.sonic.v1", "rgsml.dsp.backend.test", gain_factor_));
    }

private:
    std::string type_id_;
    rgsml::dsp::ModuleDescriptor descriptor_;
    std::int64_t latency_frames_;
    double gain_factor_;
};

[[nodiscard]] rgsml::dsp::ModuleDescriptor make_fake_descriptor(
    std::string_view type_id)
{
    const rgsml::dsp::ModuleDescriptorSpec spec{
        std::string(type_id),
        "Fake Latency Module",
        {rgsml::dsp::ModuleCategory::DYNAMICS},
        {rgsml::dsp::ProcessingStage::MASTER},
        {rgsml::dsp::ChainSegment::MANUAL},
        true,
        true,
        rgsml::dsp::PlacementClass::INLINE_CHAIN,
        std::nullopt,
        {},
        {},
        {},
        {},
        false,
        false,
        "1.0.0",
        "rgsml.dsp.fake-latency.parameters/1.0.0"
    };
    auto res = rgsml::dsp::ModuleDescriptor::create(spec);
    Q_ASSERT(res);
    return *res.value();
}

[[nodiscard]] rgsml::dsp::ModuleRegistry create_test_registry_with_fakes(
    std::int64_t latency_a = 10,
    std::int64_t latency_b = 20,
    double gain_a = 1.0,
    double gain_b = 1.0)
{
    auto base_pkg = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    Q_ASSERT(base_pkg);

    const auto& gain_d = base_pkg.value()->find_descriptor("rgsml.dsp.gain").value()->get();
    const auto& eq_d = base_pkg.value()->find_descriptor("rgsml.dsp.parametric-eq").value()->get();

    auto desc_a = make_fake_descriptor("rgsml.dsp.fake-latency-a");
    auto desc_b = make_fake_descriptor("rgsml.dsp.fake-latency-b");

    auto factory_a = std::make_shared<FakeModuleFactory>(desc_a, latency_a, gain_a);
    auto factory_b = std::make_shared<FakeModuleFactory>(desc_b, latency_b, gain_b);

    std::vector<rgsml::dsp::ModuleRegistration> regs;
    regs.push_back(rgsml::dsp::ModuleRegistration{gain_d, std::make_shared<FakeModuleFactory>(gain_d, 0, 1.0)});
    regs.push_back(rgsml::dsp::ModuleRegistration{eq_d, std::make_shared<FakeModuleFactory>(eq_d, 0, 1.0)});
    regs.push_back(rgsml::dsp::ModuleRegistration{desc_a, factory_a});
    regs.push_back(rgsml::dsp::ModuleRegistration{desc_b, factory_b});

    auto reg_res = rgsml::dsp::ModuleRegistry::create(regs);
    Q_ASSERT(reg_res);
    return std::move(*reg_res.value());
}

class RenderPreviewTest final : public QObject {
    Q_OBJECT

private slots:
    void validatesBindingsAndWindowAtomically();
    void emptyAndBypassedChainsAreExactIdentity();
    void activeGainIsChunkInvariantAndSourceImmutable();
    void multipleGainsUseFrozenSnapshotOrder();
    void requiredListeningPartitionsMatchForAllGains();
    void activeUnavailableModuleFails();
    void resultLifetimeIsIndependent();
    void parametricEqPreviewAndCausalPrerollEquivalence();
    void mixedGainAndEqOrderAndBypassIdentity();
    void activeMixedGainAndEqChainIntegration();
    void bindingOrderInvariance();
    void eqSignatureContent();
    void errorOrderingActiveUnsupportedModuleFailsTruthfully();

    // Stage 1 Mandatory Latency & Checkpoint Tests
    void latencyFakeSingleModuleCases();
    void latencyFakeTwoActiveModules();
    void latencyFakeChunkedFinalizeAndBlockSizes();
    void latencyFakePreviewNearEos();
    void latencyFakeBypassedModule();
    void checkpointAndTimelineRules();

    // Stage 2 Production Compressor Integration Tests
    void compressorRenderPreviewChainIntegration();
    void twoActiveCompressorsInChain();
    void bypassedCompressorIntegration();
    void compressorNearEosPreview();
    void monoCompressorExecutionSignature();
    void compressorTelemetryMemoryLimitFailClosed();
};

void RenderPreviewTest::validatesBindingsAndWindowAtomically()
{
    const std::array samples{0.25, -0.5, 1.0};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 100, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("20000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));

    auto missing = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain, {}, frame_count(7));
    QVERIFY(!missing);

    const rgsml::dsp::ModuleExecutionBinding binding{id, gain(6.0)};
    auto duplicate = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding, binding}, frame_count(7));
    QVERIFY(!duplicate);

    auto outside = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(99, 103), chain,
        {binding}, frame_count(7));
    QVERIFY(!outside);

    auto zero_block = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding}, frame_count(0));
    QVERIFY(!zero_block);

    // Orphan binding
    const auto orphan_id = make_id("20000000-0000-0000-0000-000000000099");
    const rgsml::dsp::ModuleExecutionBinding orphan_binding{orphan_id, gain(0.0)};
    auto orphan_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding, orphan_binding}, frame_count(7));
    QVERIFY(!orphan_req);

    // Payload mismatch
    const auto eq_id = make_id("20000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));
    const rgsml::dsp::ModuleExecutionBinding mismatch_binding{eq_id, gain(0.0)};
    auto mismatch_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(100, 103), chain,
        {binding, mismatch_binding}, frame_count(7));
    QVERIFY(!mismatch_req);
}

void RenderPreviewTest::emptyAndBypassedChainsAreExactIdentity()
{
    const std::array left{0.0, -0.0, 0.5, -1.25, std::bit_cast<double>(UINT64_C(1))};
    const std::array right{-0.0, 0.0, -0.5, 1.25, std::bit_cast<double>(UINT64_C(0x8000000000000001))};
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 50, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    for (const auto block : {1, 7, 64, 257}) {
        auto request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(50, 55), chain, {}, frame_count(block));
        QVERIFY(request);
        auto result = rgsml::render::render_preview(*request.value(), *registry.value());
        QVERIFY(result);
        QCOMPARE(bits(result.value()->view()), bits(source.value()->view()));
    }

    auto partial_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(51, 54), chain, {}, frame_count(7));
    auto partial_result = rgsml::render::render_preview(
        *partial_request.value(), *registry.value());
    auto source_partial = source.value()->view().subview(
        rgsml::core::FrameIndex{51}, frame_count(3));
    QVERIFY(partial_result);
    QCOMPARE(partial_result.value()->view().absolute_range(), frame_range(51, 54));
    QCOMPARE(bits(partial_result.value()->view()), bits(*source_partial.value()));

    const auto id = make_id("21000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.set_user_bypass(id, true));
    auto request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(50, 55), chain,
        {{id, gain(24.0)}}, frame_count(1));
    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(result);
    QCOMPARE(bits(result.value()->view()), bits(source.value()->view()));
    QCOMPARE(result.value()->signatures().size(), std::size_t{1});
    QCOMPARE(
        result.value()->signatures().front().disposition,
        rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY);
}

void RenderPreviewTest::activeGainIsChunkInvariantAndSourceImmutable()
{
    const std::array samples{
        0.0, -0.0, 0.125, -0.25, 0.5, -1.0, 1.25, -2.0,
        0.03125, -0.0625, 0.75, -0.875, 0.2, -0.4, 0.8, -1.6, 0.1};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 10, samples);
    const auto before = bits(source.value()->view());
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("22000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));

    std::vector<std::uint64_t> reference;
    for (const auto block : {1, 7, 64, 257}) {
        auto request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(10, 27), chain,
            {{id, gain(6.0)}}, frame_count(block));
        auto result = rgsml::render::render_preview(*request.value(), *registry.value());
        QVERIFY(result);
        if (reference.empty()) {
            reference = bits(result.value()->view());
        } else {
            QCOMPARE(bits(result.value()->view()), reference);
        }
        QCOMPARE(result.value()->render_window(), frame_range(10, 27));
        QCOMPARE(result.value()->frame_domain_id(), rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE);
        QCOMPARE(result.value()->chain_revision(), std::uint64_t{1});
    }
    QCOMPARE(bits(source.value()->view()), before);
}

void RenderPreviewTest::multipleGainsUseFrozenSnapshotOrder()
{
    const std::array samples{0.5, -0.25, 1.25, -2.0};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto first = make_id("23000000-0000-0000-0000-000000000001");
    const auto second = make_id("23000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(first, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(second, "rgsml.dsp.gain", 1));

    auto request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 4), chain,
        {{first, gain(6.0)}, {second, gain(-12.0)}}, frame_count(7));
    QVERIFY(request);
    QVERIFY(chain.set_user_bypass(first, true));

    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(result);
    const auto actual = *result.value()->view().channel(0).value();
    const auto plus_six = 1.9952623149688795;
    const auto minus_twelve = 0.251188643150958;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        QCOMPARE(actual[index], (samples[index] * plus_six) * minus_twelve);
    }
    QCOMPARE(result.value()->signatures().size(), std::size_t{2});
    QCOMPARE(result.value()->signatures()[0].instance_id, first);
    QCOMPARE(result.value()->signatures()[1].instance_id, second);
}

void RenderPreviewTest::requiredListeningPartitionsMatchForAllGains()
{
    std::vector<double> samples(600U);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        samples[index] = static_cast<double>(
            static_cast<std::int64_t>(index % 97U) - 48) / 128.0;
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("23500000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.gain", 0));

    for (const auto gain_db : {0.0, 6.0, -12.0}) {
        std::vector<std::uint64_t> reference;
        for (const auto block : {64, 257}) {
            auto request = rgsml::render::RenderRequest::create(
                source.value()->view(), frame_range(0, 600), chain,
                {{id, gain(gain_db)}}, frame_count(block));
            auto result = rgsml::render::render_preview(
                *request.value(), *registry.value());
            QVERIFY(result);
            if (reference.empty()) {
                reference = bits(result.value()->view());
            } else {
                QCOMPARE(bits(result.value()->view()), reference);
            }
        }
    }
}

void RenderPreviewTest::activeUnavailableModuleFails()
{
    const std::array samples{0.25};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    QVERIFY(chain.add(
        make_id("24000000-0000-0000-0000-000000000001"),
        "rgsml.dsp.stereo-ms", 0)); // Note: stereo-ms has no factory
    auto request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1), chain, {}, frame_count(1));
    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(!result);
    QCOMPARE(result.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
}

void RenderPreviewTest::resultLifetimeIsIndependent()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto result = [&registry] {
        const std::array samples{0.25, -0.5};
        auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
        auto chain = empty_chain(*registry.value());
        auto request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 2), chain, {}, frame_count(64));
        return rgsml::render::render_preview(*request.value(), *registry.value());
    }();
    QVERIFY(result);
    QCOMPARE(
        bits(result.value()->view()),
        std::vector<std::uint64_t>({
            std::bit_cast<std::uint64_t>(0.25),
            std::bit_cast<std::uint64_t>(-0.5)}));
}

void RenderPreviewTest::parametricEqPreviewAndCausalPrerollEquivalence()
{
    std::vector<double> left(1000U);
    std::vector<double> right(1000U);
    for (std::size_t i = 0; i < 1000U; ++i) {
        left[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    const auto source_bits_before = bits(source.value()->view());
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());
    const auto eq_id = make_id("25000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 0));

    const auto eq_params = bell_eq(1000.0, 6.0, 1.414);
    const rgsml::dsp::ModuleExecutionBinding binding{eq_id, eq_params};

    auto full_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1000), chain,
        {binding}, frame_count(64));
    QVERIFY(full_req);
    auto full_res = rgsml::render::render_preview(*full_req.value(), *registry.value());
    QVERIFY(full_res);

    auto full_subview = full_res.value()->view().subview(
        rgsml::core::FrameIndex{500}, frame_count(500));
    QVERIFY(full_subview);
    const auto expected_bits = bits(*full_subview.value());

    for (const auto block : {1, 7, 64, 257}) {
        auto mid_req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(500, 1000), chain,
            {binding}, frame_count(block));
        QVERIFY(mid_req);
        auto mid_res = rgsml::render::render_preview(*mid_req.value(), *registry.value());
        QVERIFY(mid_res);
        QCOMPARE(bits(mid_res.value()->view()), expected_bits);
    }

    QCOMPARE(bits(source.value()->view()), source_bits_before);
}

void RenderPreviewTest::mixedGainAndEqOrderAndBypassIdentity()
{
    const std::array left{0.5, -0.25, 0.75, -0.5};
    const std::array right{-0.5, 0.25, -0.75, 0.5};
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto gain_id = make_id("26000000-0000-0000-0000-000000000001");
    const auto eq_id = make_id("26000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));

    const auto gain_params = gain(6.0);
    const auto eq_params = bell_eq(1000.0, 3.0, 0.707);

    QVERIFY(chain.set_user_bypass(gain_id, true));
    QVERIFY(chain.set_user_bypass(eq_id, true));

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 4), chain,
        {{gain_id, gain_params}, {eq_id, eq_params}}, frame_count(64));
    QVERIFY(req);
    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);
    QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));
    QCOMPARE(res.value()->signatures().size(), std::size_t{2});
    QCOMPARE(res.value()->signatures()[0].disposition, rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY);
    QCOMPARE(res.value()->signatures()[1].disposition, rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY);
}

void RenderPreviewTest::activeMixedGainAndEqChainIntegration()
{
    std::vector<double> left(100U);
    std::vector<double> right(100U);
    for (std::size_t i = 0; i < 100U; ++i) {
        left[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto gain_id = make_id("28000000-0000-0000-0000-000000000001");
    const auto eq_id = make_id("28000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));

    const rgsml::dsp::ModuleExecutionBinding gain_binding{gain_id, gain(6.0)};
    const rgsml::dsp::ModuleExecutionBinding eq_binding{eq_id, bell_eq(1000.0, 6.0, 1.414)};

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {gain_binding, eq_binding}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);

    QVERIFY(bits(res.value()->view()) != bits(source.value()->view()));

    const auto& sigs = res.value()->signatures();
    QCOMPARE(sigs.size(), std::size_t{2});

    QCOMPARE(sigs[0].instance_id, gain_id);
    QCOMPARE(sigs[0].type_id, std::string{"rgsml.dsp.gain"});
    QCOMPARE(sigs[0].disposition, rgsml::render::ModuleExecutionDisposition::PROCESSED);

    QCOMPARE(sigs[1].instance_id, eq_id);
    QCOMPARE(sigs[1].type_id, std::string{"rgsml.dsp.parametric-eq"});
    QCOMPARE(sigs[1].disposition, rgsml::render::ModuleExecutionDisposition::PROCESSED);
}

void RenderPreviewTest::bindingOrderInvariance()
{
    std::vector<double> left(100U);
    std::vector<double> right(100U);
    for (std::size_t i = 0; i < 100U; ++i) {
        left[i] = std::sin(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * M_PI * 1000.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto gain_id = make_id("29000000-0000-0000-0000-000000000001");
    const auto eq_id = make_id("29000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));

    const rgsml::dsp::ModuleExecutionBinding gain_binding{gain_id, gain(6.0)};
    const rgsml::dsp::ModuleExecutionBinding eq_binding{eq_id, bell_eq(1000.0, 6.0, 1.414)};

    auto req_forward = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {gain_binding, eq_binding}, frame_count(64));
    QVERIFY(req_forward);
    auto res_forward = rgsml::render::render_preview(*req_forward.value(), *registry.value());
    QVERIFY(res_forward);

    auto req_reversed = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {eq_binding, gain_binding}, frame_count(64));
    QVERIFY(req_reversed);
    auto res_reversed = rgsml::render::render_preview(*req_reversed.value(), *registry.value());
    QVERIFY(res_reversed);

    QCOMPARE(bits(res_reversed.value()->view()), bits(res_forward.value()->view()));

    const auto& sigs = res_reversed.value()->signatures();
    QCOMPARE(sigs.size(), std::size_t{2});
    QCOMPARE(sigs[0].instance_id, gain_id);
    QCOMPARE(sigs[1].instance_id, eq_id);
}

void RenderPreviewTest::eqSignatureContent()
{
    const auto band_enabled_id = *rgsml::core::Uuid::parse("30000000-0000-0000-0000-000000000001").value();
    const auto band_disabled_id = *rgsml::core::Uuid::parse("30000000-0000-0000-0000-000000000002").value();

    auto enabled_band = rgsml::dsp::EqBandParameters::create(
        band_enabled_id,
        true,
        rgsml::dsp::EqFilterType::BELL,
        rgsml::dsp::EqRouting::STEREO,
        rgsml::dsp::BellPayload{1000.0, 6.0, 0.707});
    QVERIFY(enabled_band);

    auto disabled_band = rgsml::dsp::EqBandParameters::create(
        band_disabled_id,
        false,
        rgsml::dsp::EqFilterType::LOW_SHELF,
        rgsml::dsp::EqRouting::LEFT,
        rgsml::dsp::ShelfPayload{100.0, -12.0, 1.0});
    QVERIFY(disabled_band);

    auto eq_params = rgsml::dsp::ParametricEqParameters::create({*enabled_band.value(), *disabled_band.value()});
    QVERIFY(eq_params);

    const std::array samples{0.25, -0.5, 0.5};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto eq_id = make_id("30000000-0000-0000-0000-000000000003");
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 0));

    const rgsml::dsp::ModuleExecutionBinding binding{eq_id, *eq_params.value()};

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 3), chain,
        {binding}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);

    const auto& sigs = res.value()->signatures();
    QCOMPARE(sigs.size(), std::size_t{1});
    QCOMPARE(sigs[0].instance_id, eq_id);

    const auto* eq_payload = std::get_if<rgsml::render::ParametricEqExecutionSignaturePayload>(&sigs[0].payload);
    QVERIFY(eq_payload != nullptr);

    QCOMPARE(eq_payload->enabled_bands.size(), std::size_t{1});

    const auto& band_sig = eq_payload->enabled_bands[0];
    QCOMPARE(band_sig.filter_type, rgsml::dsp::EqFilterType::BELL);
    QCOMPARE(band_sig.routing, rgsml::dsp::EqRouting::STEREO);

    const auto* bell = std::get_if<rgsml::dsp::BellPayload>(&band_sig.payload);
    QVERIFY(bell != nullptr);
    QCOMPARE(bell->frequency_hz, 1000.0);
    QCOMPARE(bell->gain_db, 6.0);
    QCOMPARE(bell->q, 0.707);
}

void RenderPreviewTest::errorOrderingActiveUnsupportedModuleFailsTruthfully()
{
    const std::array samples{0.25};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    auto chain = empty_chain(*registry.value());

    const auto comp_id = make_id("27000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(comp_id, "rgsml.dsp.stereo-ms", 0)); // Note: stereo-ms has no factory

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1), chain,
        {}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(!res);
    QCOMPARE(res.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
    QCOMPARE(std::string_view{res.error()->message()}, std::string_view{"An active chain node has no production implementation."});
}

void RenderPreviewTest::latencyFakeSingleModuleCases()
{
    const std::int64_t L = 10;
    auto registry = create_test_registry_with_fakes(L, 20);

    // Case 1: N = 0
    {
        auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, {});
        auto chain = empty_chain(registry);
        const auto id = make_id("40000000-0000-0000-0000-000000000001");
        QVERIFY(chain.add(id, "rgsml.dsp.fake-latency-a", 0));

        auto req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 0), chain, {}, frame_count(64));
        QVERIFY(req);
        auto res = rgsml::render::render_preview(*req.value(), registry);
        QVERIFY(res);
        QCOMPARE(res.value()->render_window(), frame_range(0, 0));
        QCOMPARE(res.value()->view().frame_count().value(), std::int64_t{0});
    }

    // Case 2: 0 < N < L (N = 4, L = 10)
    {
        const std::array samples{0.1, 0.2, 0.3, 0.4};
        auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
        auto chain = empty_chain(registry);
        const auto id = make_id("40000000-0000-0000-0000-000000000002");
        QVERIFY(chain.add(id, "rgsml.dsp.fake-latency-a", 0));

        auto req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 4), chain, {}, frame_count(64));
        QVERIFY(req);
        auto res = rgsml::render::render_preview(*req.value(), registry);
        QVERIFY(res);
        QCOMPARE(res.value()->view().frame_count().value(), std::int64_t{4});
        QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));
    }

    // Case 3: N = L (N = 10, L = 10)
    {
        const std::array samples{0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
        auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
        auto chain = empty_chain(registry);
        const auto id = make_id("40000000-0000-0000-0000-000000000003");
        QVERIFY(chain.add(id, "rgsml.dsp.fake-latency-a", 0));

        auto req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 10), chain, {}, frame_count(64));
        QVERIFY(req);
        auto res = rgsml::render::render_preview(*req.value(), registry);
        QVERIFY(res);
        QCOMPARE(res.value()->view().frame_count().value(), std::int64_t{10});
        QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));
    }

    // Case 4: N > L (N = 25, L = 10)
    {
        std::vector<double> samples(25U);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            samples[i] = static_cast<double>(i + 1) * 0.05;
        }
        auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
        auto chain = empty_chain(registry);
        const auto id = make_id("40000000-0000-0000-0000-000000000004");
        QVERIFY(chain.add(id, "rgsml.dsp.fake-latency-a", 0));

        auto req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 25), chain, {}, frame_count(64));
        QVERIFY(req);
        auto res = rgsml::render::render_preview(*req.value(), registry);
        QVERIFY(res);
        QCOMPARE(res.value()->view().frame_count().value(), std::int64_t{25});
        QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));
    }
}

void RenderPreviewTest::latencyFakeTwoActiveModules()
{
    const std::int64_t L1 = 10;
    const std::int64_t L2 = 15;
    auto registry = create_test_registry_with_fakes(L1, L2, 2.0, 0.5);

    std::vector<double> samples(50U);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<double>(i + 1) * 0.02;
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto chain = empty_chain(registry);

    const auto id1 = make_id("41000000-0000-0000-0000-000000000001");
    const auto id2 = make_id("41000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(id1, "rgsml.dsp.fake-latency-a", 0));
    QVERIFY(chain.add(id2, "rgsml.dsp.fake-latency-b", 1));

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 50), chain, {}, frame_count(64));
    QVERIFY(req);
    auto res = rgsml::render::render_preview(*req.value(), registry);
    QVERIFY(res);
    QCOMPARE(res.value()->view().frame_count().value(), std::int64_t{50});
    QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));
}

void RenderPreviewTest::latencyFakeChunkedFinalizeAndBlockSizes()
{
    const std::int64_t L = 30;
    auto registry = create_test_registry_with_fakes(L, 10);

    std::vector<double> samples(100U);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto chain = empty_chain(registry);

    const auto id = make_id("42000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.fake-latency-a", 0));

    std::vector<std::uint64_t> reference;
    for (const auto block : {7, 13, 31, 64, 256}) {
        auto req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 100), chain, {}, frame_count(block));
        QVERIFY(req);
        auto res = rgsml::render::render_preview(*req.value(), registry);
        QVERIFY(res);
        if (reference.empty()) {
            reference = bits(res.value()->view());
        } else {
            QCOMPARE(bits(res.value()->view()), reference);
        }
    }
}

void RenderPreviewTest::latencyFakePreviewNearEos()
{
    const std::int64_t L1 = 30;
    const std::int64_t L2 = 20;
    auto registry = create_test_registry_with_fakes(L1, L2);

    std::vector<double> samples(100U);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = static_cast<double>(i + 1) * 0.01;
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto chain = empty_chain(registry);

    const auto id1 = make_id("43000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id1, "rgsml.dsp.fake-latency-a", 0));

    auto full_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain, {}, frame_count(64));
    QVERIFY(full_req);
    auto full_res = rgsml::render::render_preview(*full_req.value(), registry);
    QVERIFY(full_res);

    auto expected_subview = full_res.value()->view().subview(
        rgsml::core::FrameIndex{80}, frame_count(15));
    QVERIFY(expected_subview);

    auto near_eos_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(80, 95), chain, {}, frame_count(7));
    QVERIFY(near_eos_req);
    auto near_eos_res = rgsml::render::render_preview(*near_eos_req.value(), registry);
    QVERIFY(near_eos_res);
    QCOMPARE(bits(near_eos_res.value()->view()), bits(*expected_subview.value()));

    const auto id2 = make_id("43000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(id2, "rgsml.dsp.fake-latency-b", 1));

    auto full_two_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain, {}, frame_count(64));
    QVERIFY(full_two_req);
    auto full_two_res = rgsml::render::render_preview(*full_two_req.value(), registry);
    QVERIFY(full_two_res);

    auto expected_two_subview = full_two_res.value()->view().subview(
        rgsml::core::FrameIndex{70}, frame_count(20));
    QVERIFY(expected_two_subview);

    auto near_eos_two_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(70, 90), chain, {}, frame_count(11));
    QVERIFY(near_eos_two_req);
    auto near_eos_two_res = rgsml::render::render_preview(*near_eos_two_req.value(), registry);
    QVERIFY(near_eos_two_res);
    QCOMPARE(bits(near_eos_two_res.value()->view()), bits(*expected_two_subview.value()));
}

void RenderPreviewTest::latencyFakeBypassedModule()
{
    const std::int64_t L = 20;
    auto registry = create_test_registry_with_fakes(L, 0);

    const std::array samples{0.5, -0.25, 1.25, -2.0};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto chain = empty_chain(registry);

    const auto id = make_id("44000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.fake-latency-a", 0));
    QVERIFY(chain.set_user_bypass(id, true));

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 4), chain, {}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), registry);
    QVERIFY(res);
    QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));
}

void RenderPreviewTest::checkpointAndTimelineRules()
{
    const auto desc = make_fake_descriptor("rgsml.dsp.fake-latency-a");
    FakeLatencyModule mod(desc, 10, "fake.sonic.v1", "rgsml.dsp.backend.test", 1.0);

    // Runtime checkpoint before prepare -> InvalidState / DSP_MODULE_NOT_PREPARED
    auto cp_before_prep = mod.runtime_checkpoint();
    QVERIFY(!cp_before_prep);
    QCOMPARE(cp_before_prep.error()->code(), rgsml::core::ErrorCode::InvalidState);

    const auto fmt = format(rgsml::audio::ChannelLayout::MONO_C);
    const rgsml::dsp::DspProcessSpec spec1{fmt, rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, *rgsml::core::FrameCount::create(512).value()};
    const rgsml::dsp::DspProcessSpec spec2{fmt, rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE, *rgsml::core::FrameCount::create(1024).value()};

    QVERIFY(mod.prepare(spec1));

    // Timeline rule 1: unbound after prepare
    const std::array in_samples{1.0, 2.0, 3.0, 4.0, 5.0};
    const std::array zero_samples{0.0, 0.0, 0.0, 0.0, 0.0};
    auto in_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, in_samples);
    auto out_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, zero_samples);

    // Process without begins_stream = true fails
    const rgsml::dsp::DspProcessContext bad_context{frame_range(0, 5), false, false};
    auto bad_proc = mod.process(in_buf.value()->view(), out_buf.value()->mutable_view(), bad_context);
    QVERIFY(!bad_proc);

    // Process with begins_stream = true binds stream
    const rgsml::dsp::DspProcessContext good_context{frame_range(0, 5), true, false};
    auto good_proc = mod.process(in_buf.value()->view(), out_buf.value()->mutable_view(), good_context);
    QVERIFY(good_proc);

    // Non-contiguous range fails
    const rgsml::dsp::DspProcessContext gap_context{frame_range(10, 15), false, false};
    auto gap_proc = mod.process(in_buf.value()->view(), out_buf.value()->mutable_view(), gap_context);
    QVERIFY(!gap_proc);

    // Runtime checkpoint creation
    auto cp_res = mod.runtime_checkpoint();
    QVERIFY(cp_res);
    const auto& cp = *cp_res.value();
    QCOMPARE(cp.module_type_id, std::string{"rgsml.dsp.fake-latency-a"});
    QCOMPARE(cp.algorithm_version, std::string{"1.0.0"});
    QCOMPARE(cp.sonic_fingerprint, std::string{"fake.sonic.v1"});
    QCOMPARE(cp.backend_identity, std::string{"rgsml.dsp.backend.test"});

    // Incompatible restore (wrong type ID)
    auto bad_cp = cp;
    bad_cp.module_type_id = "rgsml.dsp.wrong";
    auto bad_rest = mod.restore_runtime_checkpoint(bad_cp);
    QVERIFY(!bad_rest);

    // Corrupt payload restore
    auto corrupt_cp = cp;
    corrupt_cp.payload.resize(2); // Truncated payload
    auto corrupt_rest = mod.restore_runtime_checkpoint(corrupt_cp);
    QVERIFY(!corrupt_rest);

    // Bound module restore next_input_frame tests:
    // Process another 5 frames on mod -> mod next_input_frame is 10
    auto in_buf2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 5, in_samples);
    auto out_buf2 = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 5, zero_samples);
    const rgsml::dsp::DspProcessContext good_context2{frame_range(5, 10), false, false};
    QVERIFY(mod.process(in_buf2.value()->view(), out_buf2.value()->mutable_view(), good_context2));

    // mod is now BOUND at next_input_frame = 10.
    // cp has next_input_frame = 5 -> restoring cp into mod (bound at 10) must be REJECTED!
    auto mismatch_next_rest = mod.restore_runtime_checkpoint(cp);
    QVERIFY(!mismatch_next_rest);

    // Unbound module (fresh prepare) accepts cp with next_input_frame = 5
    QVERIFY(mod.prepare(spec2)); // Re-prepare with spec2 (unbound, 1024 max block frames)
    auto good_rest = mod.restore_runtime_checkpoint(cp);
    QVERIFY(good_rest);

    // Checkpoint during/after finalize rejected
    const rgsml::dsp::DspProcessContext fin_context{frame_range(5, 15), false, true};
    const std::vector<double> ten_zeros(10U, 0.0);
    auto fin_buf = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 5, ten_zeros);
    auto fin_status = mod.finalize(fin_buf.value()->mutable_view(), fin_context);
    QVERIFY(fin_status);

    auto cp_in_fin = mod.runtime_checkpoint();
    QVERIFY(!cp_in_fin);
    QCOMPARE(cp_in_fin.error()->code(), rgsml::core::ErrorCode::UnsupportedOperation);
    QCOMPARE(error_category(*cp_in_fin.error()), std::string_view{"RUNTIME_CHECKPOINT_DURING_FINALIZE_UNSUPPORTED"});
    QCOMPARE(std::string_view{cp_in_fin.error()->message()}, std::string_view{"Checkpoint during or after finalize rejected"});
}

void RenderPreviewTest::compressorRenderPreviewChainIntegration()
{
    // Gain -> Compressor -> EQ
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    std::vector<double> left(1000U);
    std::vector<double> right(1000U);
    for (std::size_t i = 0; i < 1000U; ++i) {
        left[i] = std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / 48000.0);
        right[i] = std::cos(2.0 * M_PI * 440.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto chain = empty_chain(*registry.value());

    const auto gain_id = make_id("50000000-0000-0000-0000-000000000001");
    const auto comp_id = make_id("50000000-0000-0000-0000-000000000002");
    const auto eq_id = make_id("50000000-0000-0000-0000-000000000003");

    QVERIFY(chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 1));
    QVERIFY(chain.add(eq_id, "rgsml.dsp.parametric-eq", 2));

    const rgsml::dsp::ModuleExecutionBinding gain_b{gain_id, gain(3.0)};
    const rgsml::dsp::ModuleExecutionBinding comp_b{comp_id, *rgsml::dsp::CompressorParameters::create_default().value()};
    const rgsml::dsp::ModuleExecutionBinding eq_b{eq_id, bell_eq(1000.0, 3.0, 1.0)};

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1000), chain,
        {gain_b, comp_b, eq_b}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);
    QCOMPARE(res.value()->render_window(), frame_range(0, 1000));
    QCOMPARE(res.value()->signatures().size(), std::size_t{3});

    const auto& sigs = res.value()->signatures();
    QCOMPARE(sigs[0].type_id, std::string{"rgsml.dsp.gain"});
    QCOMPARE(sigs[1].type_id, std::string{"rgsml.dsp.compressor"});
    QCOMPARE(sigs[2].type_id, std::string{"rgsml.dsp.parametric-eq"});

    const auto* comp_sig = std::get_if<rgsml::render::CompressorExecutionSignaturePayload>(&sigs[1].payload);
    QVERIFY(comp_sig != nullptr);
    QCOMPARE(comp_sig->detector_mode, rgsml::dsp::CompressorDetectorMode::RMS);
    QCOMPARE(comp_sig->channel_link, std::optional<rgsml::dsp::CompressorChannelLink>{rgsml::dsp::CompressorChannelLink::LINKED_MAX});
}

void RenderPreviewTest::twoActiveCompressorsInChain()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    std::vector<double> left(500U, 0.5);
    std::vector<double> right(500U, 0.5);
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto chain = empty_chain(*registry.value());

    const auto comp1_id = make_id("51000000-0000-0000-0000-000000000001");
    const auto comp2_id = make_id("51000000-0000-0000-0000-000000000002");

    QVERIFY(chain.add(comp1_id, "rgsml.dsp.compressor", 0));
    QVERIFY(chain.add(comp2_id, "rgsml.dsp.compressor", 1));

    const rgsml::dsp::ModuleExecutionBinding comp1_b{comp1_id, *rgsml::dsp::CompressorParameters::create_default().value()};
    const rgsml::dsp::ModuleExecutionBinding comp2_b{comp2_id, *rgsml::dsp::CompressorParameters::create_default().value()};

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 500), chain,
        {comp1_b, comp2_b}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);
    QCOMPARE(res.value()->render_window(), frame_range(0, 500));
    QCOMPARE(res.value()->signatures().size(), std::size_t{2});

    const auto& sigs = res.value()->signatures();
    QCOMPARE(sigs[0].disposition, rgsml::render::ModuleExecutionDisposition::PROCESSED);
    QCOMPARE(sigs[1].disposition, rgsml::render::ModuleExecutionDisposition::PROCESSED);
}

void RenderPreviewTest::bypassedCompressorIntegration()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    std::vector<double> left{0.1, -0.2, 0.3, -0.4, 0.5};
    std::vector<double> right{-0.1, 0.2, -0.3, 0.4, -0.5};
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto chain = empty_chain(*registry.value());

    const auto comp_id = make_id("52000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 0));
    QVERIFY(chain.set_user_bypass(comp_id, true));

    const rgsml::dsp::ModuleExecutionBinding comp_b{comp_id, *rgsml::dsp::CompressorParameters::create_default().value()};

    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 5), chain,
        {comp_b}, frame_count(64));
    QVERIFY(req);

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);
    QCOMPARE(bits(res.value()->view()), bits(source.value()->view()));

    const auto& sigs = res.value()->signatures();
    QCOMPARE(sigs.size(), std::size_t{1});
    QCOMPARE(sigs[0].disposition, rgsml::render::ModuleExecutionDisposition::BYPASS_IDENTITY);
}

void RenderPreviewTest::compressorNearEosPreview()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    std::vector<double> left(1000U, 0.2);
    std::vector<double> right(1000U, -0.2);
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto chain = empty_chain(*registry.value());

    const auto comp_id = make_id("53000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 0));

    const rgsml::dsp::ModuleExecutionBinding comp_b{comp_id, *rgsml::dsp::CompressorParameters::create_default().value()};

    // Full render
    auto full_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1000), chain,
        {comp_b}, frame_count(64));
    QVERIFY(full_req);
    auto full_res = rgsml::render::render_preview(*full_req.value(), *registry.value());
    QVERIFY(full_res);

    auto expected_sub = full_res.value()->view().subview(
        rgsml::core::FrameIndex{900}, frame_count(100));
    QVERIFY(expected_sub);

    // Near-EOS preview [900, 1000]
    auto near_eos_req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(900, 1000), chain,
        {comp_b}, frame_count(32));
    QVERIFY(near_eos_req);
    auto near_eos_res = rgsml::render::render_preview(*near_eos_req.value(), *registry.value());
    QVERIFY(near_eos_res);

    QCOMPARE(near_eos_res.value()->view().frame_count().value(), std::int64_t{100});
    QCOMPARE(bits(near_eos_res.value()->view()), bits(*expected_sub.value()));
}

void RenderPreviewTest::compressorTelemetryMemoryLimitFailClosed()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    std::vector<double> mono_samples(100U, 0.25);
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, mono_samples);
    auto chain = empty_chain(*registry.value());

    const auto comp_id = make_id("55000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 0));

    const rgsml::dsp::ModuleExecutionBinding comp_b{comp_id, *rgsml::dsp::CompressorParameters::create_default().value()};

    // Baseline request without telemetry memory restriction
    auto req_baseline = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {comp_b}, frame_count(64));
    QVERIFY(req_baseline);
    auto res_baseline = rgsml::render::render_preview(*req_baseline.value(), *registry.value());
    QVERIFY(res_baseline);
    QVERIFY(res_baseline.value()->compressor_telemetry_sidecar().has_value());
    QVERIFY(res_baseline.value()->compressor_telemetry_sidecar()->valid);

    // Render request with tight 100 byte telemetry budget seam
    auto req_restricted = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {comp_b}, frame_count(64), std::size_t{100});
    QVERIFY(req_restricted);

    // Render preview succeeds unchanged and returns bit-identical audio!
    auto res_restricted = rgsml::render::render_preview(*req_restricted.value(), *registry.value());
    QVERIFY(res_restricted);
    QCOMPARE(bits(res_restricted.value()->view()), bits(res_baseline.value()->view()));

    // Sidecar is present but marked UNAVAILABLE due to telemetry memory budget fail-closed
    QVERIFY(res_restricted.value()->compressor_telemetry_sidecar().has_value());
    const auto& sidecar = *res_restricted.value()->compressor_telemetry_sidecar();
    QVERIFY(!sidecar.valid);
    QCOMPARE(sidecar.status, rgsml::render::CompressorTelemetryStatus::UNAVAILABLE);
}

void RenderPreviewTest::monoCompressorExecutionSignature()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    const std::array mono_samples{0.25, -0.5, 0.25};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, mono_samples);
    auto chain = empty_chain(*registry.value());

    const auto comp_id = make_id("54000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 0));

    for (const auto link_mode : {rgsml::dsp::CompressorChannelLink::LINKED_MAX, rgsml::dsp::CompressorChannelLink::LINKED_MEAN, rgsml::dsp::CompressorChannelLink::DUAL_MONO}) {
        auto comp_params = *rgsml::dsp::CompressorParameters::create(
            rgsml::dsp::CompressorDetectorMode::RMS, link_mode,
            -24.0, 2.0, 6.0, 30.0, 200.0, 50.0, 5.0, 100.0, 0.0).value();

        const rgsml::dsp::ModuleExecutionBinding comp_b{comp_id, comp_params};

        auto req = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 3), chain,
            {comp_b}, frame_count(64));
        QVERIFY(req);

        auto res = rgsml::render::render_preview(*req.value(), *registry.value());
        QVERIFY(res);

        const auto& sigs = res.value()->signatures();
        QCOMPARE(sigs.size(), std::size_t{1});

        const auto* comp_sig = std::get_if<rgsml::render::CompressorExecutionSignaturePayload>(&sigs[0].payload);
        QVERIFY(comp_sig != nullptr);
        // Effective link MUST be std::nullopt for MONO_C layout!
        QCOMPARE(comp_sig->channel_link, std::optional<rgsml::dsp::CompressorChannelLink>{std::nullopt});
    }
}

}  // namespace
}  // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::RenderPreviewTest)

#include "test_render_preview.moc"
