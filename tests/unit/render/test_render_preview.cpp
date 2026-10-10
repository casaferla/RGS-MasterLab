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
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <rgsml/render/audible_compressor_telemetry_resolver.hpp>
#include <rgsml/render/compressor_telemetry_collector.hpp>
#include <rgsml/render/compressor_telemetry_history.hpp>
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
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
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
    void stereoMsTypedBindingsRejectMissingWrongAndDuplicate();
    void stereoMsBypassAndMonoProduceExactIdentity();
    void stereoMsOffAndSideMuteUseTypedNonDefaults();
    void stereoMsCrossoverPartitionAndPreroll();
    void stereoMsMixedChainPreservesCompressorTelemetry();
    void stereoMsStageOutputCaptureBoundedAndProvenanced();
    void stereoMsLocalCorrelationCanonicalGrid100();
    void stereoMsSideLowRealBranchRmsGrid100();
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

    // Stage 3B1 Telemetry Seam & Bounded History Tests
    void telemetryHistorySeamAndRealizationIdentity();

    // Stage 3B2 Audible Telemetry Resolver Contract Tests
    void audibleCompressorTelemetryResolverContract();
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

void RenderPreviewTest::stereoMsTypedBindingsRejectMissingWrongAndDuplicate()
{
    const std::array samples{0.25, -0.5};
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, samples);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("24000000-0000-0000-0000-000000000003");
    QVERIFY(chain.add(id, "rgsml.dsp.stereo-ms", 0));
    auto params = rgsml::dsp::StereoMsParameters::create(
        -3.0, 8.0, false, rgsml::dsp::MonoBassMode::LR24, 300.0, 25.0);
    QVERIFY(params);
    const rgsml::dsp::ModuleExecutionBinding binding{id, *params.value()};

    for (const bool bypass : {false, true}) {
        if (bypass) QVERIFY(chain.set_user_bypass(id, true));

        auto missing = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 2),
            chain, {}, frame_count(7));
        QVERIFY(!missing);
        QCOMPARE(missing.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
        QCOMPARE(std::string_view{missing.error()->message()},
                 std::string_view{
                    "Every supported parameterized module snapshot entry requires exactly one binding."});

        auto wrong = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 2),
            chain, {{id, gain(6.0)}}, frame_count(7));
        QVERIFY(!wrong);
        QCOMPARE(wrong.error()->code(), rgsml::core::ErrorCode::InvalidArgument);
        QCOMPARE(std::string_view{wrong.error()->message()},
                 std::string_view{
                    "A Stereo/M-S binding contained a non-StereoMsParameters payload."});

        auto duplicate = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 2),
            chain, {binding, binding}, frame_count(7));
        QVERIFY(!duplicate);
        QCOMPARE(std::string_view{duplicate.error()->message()},
                 std::string_view{
                    "A module instance has more than one parameter binding."});

        auto valid = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 2),
            chain, {binding}, frame_count(7));
        QVERIFY(valid);
        QCOMPARE(valid.value()->bindings().size(), std::size_t{1});
        QVERIFY(valid.value()->bindings().front().parameters
                == binding.parameters);

        // The valid typed mono binding must now render bit-exact identity,
        // not fall back to generic Stereo/M-S defaults.
        auto rendered = rgsml::render::render_preview(
            *valid.value(), *registry.value());
        QVERIFY(rendered);
        QCOMPARE(bits(rendered.value()->view()), bits(source.value()->view()));
        QCOMPARE(rendered.value()->signatures().size(), std::size_t{1});
        const auto* signature = std::get_if<rgsml::render::StereoMsExecutionSignaturePayload>(
            &rendered.value()->signatures().front().payload);
        QVERIFY(signature != nullptr);
        QVERIFY(!signature->mid_gain_db && !signature->side_gain_db
                && !signature->side_muted && !signature->mono_bass_mode
                && !signature->mono_bass_cutoff_hz && !signature->low_band_width_percent);
    }
}

void RenderPreviewTest::stereoMsBypassAndMonoProduceExactIdentity()
{
    using rgsml::dsp::MonoBassMode;
    using rgsml::render::ModuleExecutionDisposition;
    using rgsml::render::StereoMsExecutionSignaturePayload;

    const std::array mono_samples{0.0, -0.0, 0.25, -0.5,
                                  std::bit_cast<double>(UINT64_C(1))};
    const std::array left{0.5, -0.0, -0.75, 0.125, 0.25};
    const std::array right{-0.25, 0.0, 0.5, -0.125, -0.5};
    auto mono = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 31, mono_samples);
    auto stereo = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 31, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(mono && stereo && registry);
    QVERIFY(registry.value()->has_factory("rgsml.dsp.stereo-ms"));
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("24000000-0000-0000-0000-000000000002");
    QVERIFY(chain.add(id, "rgsml.dsp.stereo-ms", 0));
    auto parameters = rgsml::dsp::StereoMsParameters::create(
        -3.0, 8.0, false, MonoBassMode::LR12, 120.0, 25.0);
    QVERIFY(parameters);

    // Even non-neutral stored fields are non-effective for canonical mono.
    for (const bool bypass : {false, true}) {
        QVERIFY(chain.set_user_bypass(id, bypass));
        auto request = rgsml::render::RenderRequest::create(
            mono.value()->view(), frame_range(31, 36), chain,
            {{id, *parameters.value()}}, frame_count(1));
        QVERIFY(request);
        auto result = rgsml::render::render_preview(*request.value(), *registry.value());
        QVERIFY(result);
        QCOMPARE(bits(result.value()->view()), bits(mono.value()->view()));
        const auto& signatures = result.value()->signatures();
        QCOMPARE(signatures.size(), std::size_t{1});
        QCOMPARE(signatures.front().instance_id, id);
        QCOMPARE(signatures.front().disposition,
                 bypass ? ModuleExecutionDisposition::BYPASS_IDENTITY
                        : ModuleExecutionDisposition::PROCESSED);
        const auto* payload = std::get_if<StereoMsExecutionSignaturePayload>(
            &signatures.front().payload);
        QVERIFY(payload != nullptr);
        QVERIFY(!payload->mid_gain_db && !payload->side_gain_db
                && !payload->side_muted && !payload->mono_bass_mode
                && !payload->mono_bass_cutoff_hz && !payload->low_band_width_percent);
    }

    // Stereo bypass must also preserve exact bits, with a semantic identity.
    auto request = rgsml::render::RenderRequest::create(
        stereo.value()->view(), frame_range(31, 36), chain,
        {{id, *parameters.value()}}, frame_count(7));
    QVERIFY(request);
    auto result = rgsml::render::render_preview(*request.value(), *registry.value());
    QVERIFY(result);
    QCOMPARE(bits(result.value()->view()), bits(stereo.value()->view()));
    QCOMPARE(result.value()->signatures().front().disposition,
             ModuleExecutionDisposition::BYPASS_IDENTITY);
    QVERIFY(!result.value()->compressor_telemetry_sidecar().has_value());
}

void RenderPreviewTest::stereoMsOffAndSideMuteUseTypedNonDefaults()
{
    using rgsml::dsp::MonoBassMode;
    using rgsml::render::ModuleExecutionDisposition;
    using rgsml::render::StereoMsExecutionSignaturePayload;
    const std::array left{0.75, 0.5, -0.25, 0.125, -0.875, 0.0};
    const std::array right{-0.25, 0.25, 0.5, -0.5, 0.125, 0.0};
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(source && registry);
    const auto source_bits = bits(source.value()->view());
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("24100000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.stereo-ms", 0));

    auto off = rgsml::dsp::StereoMsParameters::create(
        -3.0, 8.0, false, MonoBassMode::OFF, 260.0, 25.0);
    QVERIFY(off);
    auto off_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 6), chain,
        {{id, *off.value()}}, frame_count(1));
    QVERIFY(off_request);
    auto off_render = rgsml::render::render_preview(
        *off_request.value(), *registry.value());
    QVERIFY(off_render);
    const auto* off_signature = std::get_if<StereoMsExecutionSignaturePayload>(
        &off_render.value()->signatures().front().payload);
    QVERIFY(off_signature != nullptr);
    QCOMPARE(off_render.value()->signatures().front().disposition,
             ModuleExecutionDisposition::PROCESSED);
    QVERIFY(off_signature->mid_gain_db == std::optional<double>{-3.0});
    QVERIFY(off_signature->side_gain_db == std::optional<double>{8.0});
    QVERIFY(off_signature->side_muted == std::optional<bool>{false});
    QVERIFY(off_signature->mono_bass_mode == std::optional{MonoBassMode::OFF});
    QVERIFY(!off_signature->mono_bass_cutoff_hz && !off_signature->low_band_width_percent);

    // Independent analytic oracle: the non-default M/S gains really reach DSP.
    constexpr double basis = 0x1.6a09e667f3bcdp-1;
    const double mid_gain = std::pow(10.0, -3.0 / 20.0);
    const double side_gain = std::pow(10.0, 8.0 / 20.0);
    const auto actual_left = *off_render.value()->view().channel(0).value();
    const auto actual_right = *off_render.value()->view().channel(1).value();
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double mid = (left[i] + right[i]) * basis * mid_gain;
        const double side = (left[i] - right[i]) * basis * side_gain;
        QVERIFY(std::abs(actual_left[i] - (mid + side) * basis) < 1e-12);
        QVERIFY(std::abs(actual_right[i] - (mid - side) * basis) < 1e-12);
    }
    auto defaults = rgsml::dsp::StereoMsParameters::create_default();
    QVERIFY(defaults);
    auto default_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 6), chain,
        {{id, *defaults.value()}}, frame_count(64));
    QVERIFY(default_request);
    auto default_render = rgsml::render::render_preview(
        *default_request.value(), *registry.value());
    QVERIFY(default_render);
    QVERIFY(bits(default_render.value()->view()) != bits(off_render.value()->view()));

    auto muted_lr = rgsml::dsp::StereoMsParameters::create(
        -3.0, -24.0, true, MonoBassMode::LR24, 190.0, 0.0);
    auto muted_off = rgsml::dsp::StereoMsParameters::create(
        -3.0, 8.0, true, MonoBassMode::OFF, 260.0, 100.0);
    QVERIFY(muted_lr && muted_off);
    auto lr_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 6), chain,
        {{id, *muted_lr.value()}}, frame_count(7));
    auto mute_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 6), chain,
        {{id, *muted_off.value()}}, frame_count(64));
    QVERIFY(lr_request && mute_request);
    auto lr_render = rgsml::render::render_preview(
        *lr_request.value(), *registry.value());
    auto mute_render = rgsml::render::render_preview(
        *mute_request.value(), *registry.value());
    QVERIFY(lr_render && mute_render);
    QCOMPARE(bits(lr_render.value()->view()), bits(mute_render.value()->view()));
    const auto output_left = *lr_render.value()->view().channel(0).value();
    const auto output_right = *lr_render.value()->view().channel(1).value();
    for (std::size_t i = 0; i < left.size(); ++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(output_left[i]),
                 std::bit_cast<std::uint64_t>(output_right[i]));
    }
    const auto* muted_signature = std::get_if<StereoMsExecutionSignaturePayload>(
        &lr_render.value()->signatures().front().payload);
    QVERIFY(muted_signature != nullptr);
    QVERIFY(muted_signature->mid_gain_db == std::optional<double>{-3.0});
    QVERIFY(muted_signature->side_muted == std::optional<bool>{true});
    QVERIFY(!muted_signature->side_gain_db && !muted_signature->mono_bass_mode
            && !muted_signature->mono_bass_cutoff_hz
            && !muted_signature->low_band_width_percent);
    QCOMPARE(bits(source.value()->view()), source_bits);
}

void RenderPreviewTest::stereoMsCrossoverPartitionAndPreroll()
{
    using rgsml::dsp::MonoBassMode;
    using rgsml::render::StereoMsExecutionSignaturePayload;
    std::vector<double> left(2048U), right(2048U);
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = 0.4 * std::sin(2.0 * M_PI * 91.0 * static_cast<double>(i) / 48000.0)
                + 0.2 * std::cos(2.0 * M_PI * 1600.0 * static_cast<double>(i) / 48000.0);
        right[i] = 0.25 * std::cos(2.0 * M_PI * 173.0 * static_cast<double>(i) / 48000.0)
                 - 0.1 * std::sin(2.0 * M_PI * 2800.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(source && registry);
    const auto source_bits = bits(source.value()->view());
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("24200000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.stereo-ms", 0));
    std::vector<std::uint64_t> off_bits, lr12_bits, lr24_bits;

    for (const auto mode : {MonoBassMode::OFF, MonoBassMode::LR12,
                            MonoBassMode::LR24}) {
        auto parameters = rgsml::dsp::StereoMsParameters::create(
            -3.0, 8.0, false, mode, 160.0, 25.0);
        QVERIFY(parameters);
        auto reference_request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 2048), chain,
            {{id, *parameters.value()}}, frame_count(64));
        QVERIFY(reference_request);
        auto reference_render = rgsml::render::render_preview(
            *reference_request.value(), *registry.value());
        QVERIFY(reference_render);
        const auto reference = bits(reference_render.value()->view());
        if (mode == MonoBassMode::OFF) off_bits = reference;
        else if (mode == MonoBassMode::LR12) lr12_bits = reference;
        else lr24_bits = reference;

        const auto* signature = std::get_if<StereoMsExecutionSignaturePayload>(
            &reference_render.value()->signatures().front().payload);
        QVERIFY(signature != nullptr);
        QCOMPARE(signature->mono_bass_mode, std::optional{mode});
        QCOMPARE(signature->mono_bass_cutoff_hz.has_value(), mode != MonoBassMode::OFF);
        QCOMPARE(signature->low_band_width_percent.has_value(), mode != MonoBassMode::OFF);

        for (const auto block : {1, 7, 257}) {
            auto request = rgsml::render::RenderRequest::create(
                source.value()->view(), frame_range(0, 2048), chain,
                {{id, *parameters.value()}}, frame_count(block));
            QVERIFY(request);
            auto rendered = rgsml::render::render_preview(
                *request.value(), *registry.value());
            QVERIFY(rendered);
            QCOMPARE(bits(rendered.value()->view()), reference);
        }
        auto mid_request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(600, 2000), chain,
            {{id, *parameters.value()}}, frame_count(31));
        QVERIFY(mid_request);
        auto mid_render = rgsml::render::render_preview(
            *mid_request.value(), *registry.value());
        QVERIFY(mid_render);
        auto expected_mid = reference_render.value()->view().subview(
            rgsml::core::FrameIndex{600}, frame_count(1400));
        QVERIFY(expected_mid);
        QCOMPARE(bits(mid_render.value()->view()), bits(*expected_mid.value()));
    }
    QVERIFY(off_bits != lr12_bits);
    QVERIFY(off_bits != lr24_bits);
    QVERIFY(lr12_bits != lr24_bits);
    QCOMPARE(bits(source.value()->view()), source_bits);
}

void RenderPreviewTest::stereoMsMixedChainPreservesCompressorTelemetry()
{
    using rgsml::dsp::MonoBassMode;
    using rgsml::render::ModuleExecutionDisposition;
    std::vector<double> left(1024U), right(1024U);
    for (std::size_t i = 0; i < left.size(); ++i) {
        left[i] = 0.35 * std::sin(2.0 * M_PI * 170.0 * static_cast<double>(i) / 48000.0);
        right[i] = 0.2 * std::cos(2.0 * M_PI * 270.0 * static_cast<double>(i) / 48000.0);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(source && registry);
    const auto before = bits(source.value()->view());
    const auto gain_id = make_id("24300000-0000-0000-0000-000000000001");
    const auto eq_id   = make_id("24300000-0000-0000-0000-000000000002");
    const auto ms_id   = make_id("24300000-0000-0000-0000-000000000003");
    const auto comp_id = make_id("24300000-0000-0000-0000-000000000004");
    auto ms = rgsml::dsp::StereoMsParameters::create(
        -3.0, 8.0, false, MonoBassMode::LR12, 150.0, 40.0);
    auto comp = rgsml::dsp::CompressorParameters::create_default();
    QVERIFY(ms && comp);
    const rgsml::dsp::ModuleExecutionBinding b_gain{gain_id, gain(6.0)};
    const rgsml::dsp::ModuleExecutionBinding b_eq{eq_id, bell_eq(1200.0, 4.0, 0.9)};
    const rgsml::dsp::ModuleExecutionBinding b_ms{ms_id, *ms.value()};
    const rgsml::dsp::ModuleExecutionBinding b_comp{comp_id, *comp.value()};

    auto complete_chain = empty_chain(*registry.value());
    QVERIFY(complete_chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(complete_chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));
    QVERIFY(complete_chain.add(ms_id, "rgsml.dsp.stereo-ms", 2));
    QVERIFY(complete_chain.add(comp_id, "rgsml.dsp.compressor", 3));
    auto full_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1024), complete_chain,
        {b_comp, b_ms, b_eq, b_gain}, frame_count(64));
    QVERIFY(full_request);
    auto full = rgsml::render::render_preview(
        *full_request.value(), *registry.value());
    QVERIFY(full);
    const auto& signatures = full.value()->signatures();
    QCOMPARE(signatures.size(), std::size_t{4});
    QCOMPARE(signatures[0].instance_id, gain_id);
    QCOMPARE(signatures[1].instance_id, eq_id);
    QCOMPARE(signatures[2].instance_id, ms_id);
    QCOMPARE(signatures[3].instance_id, comp_id);
    for (const auto& signature : signatures) {
        QCOMPARE(signature.disposition, ModuleExecutionDisposition::PROCESSED);
    }
    QVERIFY(full.value()->compressor_telemetry_sidecar().has_value());
    QVERIFY(full.value()->compressor_telemetry_sidecar()->valid);
    QCOMPARE(full.value()->stereo_ms_stage_output_sidecars().size(), std::size_t{1});
    const auto& stage = full.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(stage.module_instance_id, ms_id);
    QCOMPARE(stage.chain_revision, full.value()->chain_revision());
    QCOMPARE(stage.status, rgsml::render::StereoMsStageCaptureStatus::COMPLETE);
    QCOMPARE(stage.requested_begin_frame, std::int64_t{0});
    QCOMPARE(stage.requested_end_frame, std::int64_t{1024});
    QCOMPARE(stage.captured_begin_frame, std::int64_t{0});
    QCOMPARE(stage.output_lr_frames.size(), std::size_t{1024});

    // Independent Render Preview cascade: Gain -> EQ -> M/S, then Compressor.
    // Comparison proves the mixed-chain processing order, not just signature order.
    auto upstream_chain = empty_chain(*registry.value());
    QVERIFY(upstream_chain.add(gain_id, "rgsml.dsp.gain", 0));
    QVERIFY(upstream_chain.add(eq_id, "rgsml.dsp.parametric-eq", 1));
    QVERIFY(upstream_chain.add(ms_id, "rgsml.dsp.stereo-ms", 2));
    auto upstream_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1024), upstream_chain,
        {b_ms, b_gain, b_eq}, frame_count(257));
    QVERIFY(upstream_request);
    auto upstream = rgsml::render::render_preview(
        *upstream_request.value(), *registry.value());
    QVERIFY(upstream);
    const auto upstream_left = *upstream.value()->view().channel(0).value();
    const auto upstream_right = *upstream.value()->view().channel(1).value();
    // This is the M/S STAGE output, even when downstream Compressor changes
    // the final sound. The two real PCM channel values must be bit-identical
    // to an independently rendered Gain -> EQ -> Stereo/M-S chain.
    for (std::size_t i = 0; i < stage.output_lr_frames.size(); ++i) {
        QCOMPARE(std::bit_cast<std::uint64_t>(stage.output_lr_frames[i][0]),
                 std::bit_cast<std::uint64_t>(upstream_left[i]));
        QCOMPARE(std::bit_cast<std::uint64_t>(stage.output_lr_frames[i][1]),
                 std::bit_cast<std::uint64_t>(upstream_right[i]));
    }
    auto intermediate = make_buffer(
        rgsml::audio::ChannelLayout::STEREO_LR, 0, upstream_left, upstream_right);
    QVERIFY(intermediate);
    auto compressor_chain = empty_chain(*registry.value());
    QVERIFY(compressor_chain.add(comp_id, "rgsml.dsp.compressor", 0));
    auto compressor_request = rgsml::render::RenderRequest::create(
        intermediate.value()->view(), frame_range(0, 1024), compressor_chain,
        {b_comp}, frame_count(7));
    QVERIFY(compressor_request);
    auto standalone = rgsml::render::render_preview(
        *compressor_request.value(), *registry.value());
    QVERIFY(standalone);
    QCOMPARE(bits(full.value()->view()), bits(standalone.value()->view()));
    QVERIFY(standalone.value()->compressor_telemetry_sidecar().has_value());
    QCOMPARE(full.value()->compressor_telemetry_sidecar()->status,
             standalone.value()->compressor_telemetry_sidecar()->status);
    QCOMPARE(bits(source.value()->view()), before);
}


void RenderPreviewTest::stereoMsStageOutputCaptureBoundedAndProvenanced()
{
    using rgsml::render::StereoMsStageCaptureStatus;
    constexpr std::size_t kFrames = 6000;
    std::vector<double> left(kFrames), right(kFrames);
    for (std::size_t i = 0; i < kFrames; ++i) {
        left[i] = 0.25 * std::sin(0.01 * static_cast<double>(i));
        right[i] = 0.125 * std::cos(0.03 * static_cast<double>(i));
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR, 0, left, right);
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(source && registry);
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("24400000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.stereo-ms", 0));
    const auto p = rgsml::dsp::StereoMsParameters::create(
        0.0, 6.0, false, rgsml::dsp::MonoBassMode::OFF, 120.0, 100.0);
    QVERIFY(p);
    const auto unchanged = bits(source.value()->view());
    const rgsml::dsp::ModuleExecutionBinding binding{id, *p.value()};

    const auto fullRequest = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 6000), chain,
        {binding}, frame_count(17));
    QVERIFY(fullRequest);
    auto full = rgsml::render::render_preview(*fullRequest.value(), *registry.value());
    QVERIFY(full);
    QCOMPARE(full.value()->stereo_ms_stage_output_sidecars().size(), std::size_t{1});
    const auto& stage = full.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(stage.status, StereoMsStageCaptureStatus::PARTIAL);
    QCOMPARE(stage.output_lr_frames.size(), std::size_t{4096});
    QCOMPARE(stage.requested_begin_frame, std::int64_t{0});
    QCOMPARE(stage.requested_end_frame, std::int64_t{6000});
    QCOMPARE(stage.captured_begin_frame, std::int64_t{0});
    QCOMPARE(stage.module_instance_id, id);
    QCOMPARE(stage.channel_layout, rgsml::audio::ChannelLayout::STEREO_LR);
    QCOMPARE(stage.frame_domain_id, rgsml::audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QCOMPARE(stage.chain_revision, full.value()->chain_revision());
    QVERIFY(!stage.realization_id);
    const auto fullLeft = *full.value()->view().channel(0).value();
    const auto fullRight = *full.value()->view().channel(1).value();
    for (std::size_t i : {std::size_t{0}, std::size_t{5}, std::size_t{4095}}) {
        QCOMPARE(std::bit_cast<std::uint64_t>(stage.output_lr_frames[i][0]),
                 std::bit_cast<std::uint64_t>(fullLeft[i]));
        QCOMPARE(std::bit_cast<std::uint64_t>(stage.output_lr_frames[i][1]),
                 std::bit_cast<std::uint64_t>(fullRight[i]));
    }

    // The excerpt must begin at the requested PLAYBACK support rather than
    // the preroll beginning (render() processes the source from frame zero).
    auto windowRequest = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(600, 2000), chain,
        {binding}, frame_count(31));
    QVERIFY(windowRequest);
    auto windowResult = rgsml::render::render_preview(
        *windowRequest.value(), *registry.value());
    QVERIFY(windowResult);
    const auto& supported = windowResult.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(supported.status, StereoMsStageCaptureStatus::COMPLETE);
    QCOMPARE(supported.requested_begin_frame, std::int64_t{600});
    QCOMPARE(supported.requested_end_frame, std::int64_t{2000});
    QCOMPARE(supported.captured_begin_frame, std::int64_t{600});
    QCOMPARE(supported.output_lr_frames.size(), std::size_t{1400});
    QCOMPARE(std::bit_cast<std::uint64_t>(supported.output_lr_frames[0][0]),
             std::bit_cast<std::uint64_t>(fullLeft[600]));

    // Explicit small/zero budgets must not impact the audible PCM or
    // misrepresent truncated/unavailable telemetry as complete.
    auto oneFrameRequest = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(600, 2000), chain,
        {binding}, frame_count(17), std::size_t{16});
    auto noFramesRequest = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(600, 2000), chain,
        {binding}, frame_count(17), std::size_t{0});
    QVERIFY(oneFrameRequest && noFramesRequest);
    auto one = rgsml::render::render_preview(
        *oneFrameRequest.value(), *registry.value());
    auto none = rgsml::render::render_preview(
        *noFramesRequest.value(), *registry.value());
    QVERIFY(one && none);
    QCOMPARE(bits(one.value()->view()), bits(windowResult.value()->view()));
    QCOMPARE(bits(none.value()->view()), bits(windowResult.value()->view()));
    const auto& partial = one.value()->stereo_ms_stage_output_sidecars().front();
    const auto& absent = none.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(partial.status, StereoMsStageCaptureStatus::PARTIAL);
    QCOMPARE(partial.output_lr_frames.size(), std::size_t{1});
    QCOMPARE(absent.status, StereoMsStageCaptureStatus::UNAVAILABLE);
    QVERIFY(absent.output_lr_frames.empty());

    // Binding to accepted Processed identity succeeds once and cannot be
    // overridden by a competing realization without mutating prior evidence.
    const rgsml::core::RealizationId acceptedId{71};
    const rgsml::core::RealizationId rejectedId{72};
    QVERIFY(windowResult.value()->bind_stereo_ms_stage_output_realization_id(acceptedId));
    QCOMPARE(windowResult.value()->stereo_ms_stage_output_sidecars().front().realization_id,
             std::optional{acceptedId});
    QVERIFY(!windowResult.value()->bind_stereo_ms_stage_output_realization_id(rejectedId));
    QCOMPARE(windowResult.value()->stereo_ms_stage_output_sidecars().front().realization_id,
             std::optional{acceptedId});

    // The source remains immutable, and bypass must not expose a stale stage
    // capture when the Stereo/M-S process was not executed.
    QCOMPARE(bits(source.value()->view()), unchanged);
    QVERIFY(chain.set_user_bypass(id, true));
    auto bypassRequest = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 6000), chain,
        {binding}, frame_count(257));
    QVERIFY(bypassRequest);
    auto bypassResult = rgsml::render::render_preview(
        *bypassRequest.value(), *registry.value());
    QVERIFY(bypassResult);
    QVERIFY(bypassResult.value()->stereo_ms_stage_output_sidecars().empty());

    QVERIFY(chain.set_user_bypass(id, false));
    const std::array<double, 2> monoSamples{0.25, -0.25};
    auto mono = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, monoSamples);
    QVERIFY(mono);
    auto monoRequest = rgsml::render::RenderRequest::create(
        mono.value()->view(), frame_range(0, 2), chain,
        {binding}, frame_count(1));
    QVERIFY(monoRequest);
    auto monoResult = rgsml::render::render_preview(
        *monoRequest.value(), *registry.value());
    QVERIFY(monoResult);
    QVERIFY(monoResult.value()->stereo_ms_stage_output_sidecars().empty());
}


void RenderPreviewTest::stereoMsLocalCorrelationCanonicalGrid100()
{
    using rgsml::render::StereoMsCorrelationStatus;
    using rgsml::render::StereoMsCorrelationWindowValidity;
    const auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("24500000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.stereo-ms", 0));
    auto neutral = rgsml::dsp::StereoMsParameters::create_default();
    QVERIFY(neutral);
    const rgsml::dsp::ModuleExecutionBinding binding{id, *neutral.value()};

    // Valid identical nonconstant stereo -> rho +1 at all FULL windows.
    const std::size_t n48 = 60'000;
    std::vector<double> left(n48);
    for (std::size_t i = 0; i < n48; ++i) {
        left[i] = 0.3 * std::sin(2.0 * M_PI * 151.0
                               * static_cast<double>(i) / 48'000.0);
    }
    auto input = make_buffer(format(rgsml::audio::ChannelLayout::STEREO_LR),
                             0, left, left);
    QVERIFY(input);
    const auto source_bits = bits(input.value()->view());
    auto req = rgsml::render::RenderRequest::create(
        input.value()->view(), frame_range(0, 60'000), chain,
        {binding}, frame_count(257));
    QVERIFY(req);
    auto result = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(result);
    QCOMPARE(result.value()->stereo_ms_stage_output_sidecars().size(), std::size_t{1});
    const auto& capture = result.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(capture.status, rgsml::render::StereoMsStageCaptureStatus::PARTIAL);
    QCOMPARE(capture.correlation_status, StereoMsCorrelationStatus::COMPLETE);
    QCOMPARE(capture.correlation_windows.size(), std::size_t{9});
    const std::array<std::int64_t, 4> expected48{0, 4800, 9600, 14400};
    for (std::size_t i = 0; i < capture.correlation_windows.size(); ++i) {
        const auto& w = capture.correlation_windows[i];
        if (i < expected48.size()) QCOMPARE(w.begin_frame, expected48[i]);
        QCOMPARE(w.end_frame - w.begin_frame, std::int64_t{19200});
        QCOMPARE(w.validity, StereoMsCorrelationWindowValidity::VALID);
        QVERIFY(w.rho.has_value());
        QVERIFY(std::abs(*w.rho - 1.0) < 1e-12);
    }
    QCOMPARE(bits(input.value()->view()), source_bits);

    // Regression beyond bit-identical channels: affine positive/negative
    // proportional stereo must retain the mathematical +/-1 endpoints,
    // even when the sqrt(E_L)*sqrt(E_R) product rounds slightly down.
    for (const double scale : {2.0, -2.0}) {
        std::vector<double> scaled_right(n48);
        for (std::size_t i = 0; i < n48; ++i) {
            scaled_right[i] = scale * left[i];
        }
        auto scaled_source = make_buffer(
            format(rgsml::audio::ChannelLayout::STEREO_LR),
            0, left, scaled_right);
        QVERIFY(scaled_source);
        auto scaled_req = rgsml::render::RenderRequest::create(
            scaled_source.value()->view(), frame_range(0, 60'000), chain,
            {binding}, frame_count(31));
        QVERIFY(scaled_req);
        auto scaled_result = rgsml::render::render_preview(
            *scaled_req.value(), *registry.value());
        QVERIFY(scaled_result);
        const auto& scaled_windows =
            scaled_result.value()->stereo_ms_stage_output_sidecars().front()
                .correlation_windows;
        QCOMPARE(scaled_windows.size(), std::size_t{9});
        for (const auto& w : scaled_windows) {
            QCOMPARE(w.validity, StereoMsCorrelationWindowValidity::VALID);
            QVERIFY(w.rho.has_value());
            QVERIFY(std::abs(*w.rho - (scale > 0 ? 1.0 : -1.0)) < 1e-12);
        }
    }

    // A tight shared telemetry budget leaves exactly one complete 400-ms
    // correlation window, truthfully reports PARTIAL and never changes PCM.
    const std::size_t one_window_budget =
        64U * 1024U + sizeof(rgsml::render::StereoMsCorrelationWindow);
    auto tight_req = rgsml::render::RenderRequest::create(
        input.value()->view(), frame_range(0, 60'000), chain,
        {binding}, frame_count(1), one_window_budget);
    QVERIFY(tight_req);
    auto tight = rgsml::render::render_preview(
        *tight_req.value(), *registry.value());
    QVERIFY(tight);
    QCOMPARE(bits(tight.value()->view()), bits(result.value()->view()));
    const auto& budgeted = tight.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(budgeted.correlation_status, StereoMsCorrelationStatus::PARTIAL);
    QCOMPARE(budgeted.correlation_windows.size(), std::size_t{1});

    // Explicit zero cap does not convert absent telemetry into rho=0.
    auto zero_req = rgsml::render::RenderRequest::create(
        input.value()->view(), frame_range(0, 60'000), chain,
        {binding}, frame_count(1024), std::size_t{0});
    QVERIFY(zero_req);
    auto zero = rgsml::render::render_preview(
        *zero_req.value(), *registry.value());
    QVERIFY(zero);
    QCOMPARE(bits(zero.value()->view()), bits(result.value()->view()));
    const auto& not_available = zero.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(not_available.correlation_status, StereoMsCorrelationStatus::UNAVAILABLE);
    QVERIFY(not_available.correlation_windows.empty());

    // 44,105 Hz is a regression guard: rounded ties-to-even Grid100 does
    // NOT have one fixed 4,410-frame hop. j=2 starts at 8,821 exactly.
    constexpr std::size_t nOdd = 50'000;
    std::vector<double> oddLeft(nOdd), oddRight(nOdd);
    for (std::size_t i = 0; i < nOdd; ++i) {
        oddLeft[i] = 0.4 * std::sin(2.0 * M_PI * 173.0
                                   * static_cast<double>(i) / 44'105.0);
        oddRight[i] = -oddLeft[i];
    }
    auto oddSource = make_buffer(
        format(rgsml::audio::ChannelLayout::STEREO_LR, 44'105),
        0, oddLeft, oddRight);
    QVERIFY(oddSource);
    auto oddReq = rgsml::render::RenderRequest::create(
        oddSource.value()->view(), frame_range(0, 50'000), chain,
        {binding}, frame_count(4096));
    QVERIFY(oddReq);
    auto odd = rgsml::render::render_preview(
        *oddReq.value(), *registry.value());
    QVERIFY(odd);
    const auto& oddCorr = odd.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(oddCorr.correlation_status, StereoMsCorrelationStatus::COMPLETE);
    QVERIFY(oddCorr.correlation_windows.size() >= std::size_t{5});
    const std::array<std::int64_t, 5> expectedOdd{0, 4410, 8821, 13232, 17642};
    for (std::size_t i = 0; i < expectedOdd.size(); ++i) {
        const auto& w = oddCorr.correlation_windows[i];
        QCOMPARE(w.begin_frame, expectedOdd[i]);
        QCOMPARE(w.end_frame - w.begin_frame, std::int64_t{17642});
        QCOMPARE(w.validity, StereoMsCorrelationWindowValidity::VALID);
        QVERIFY(w.rho.has_value());
        QVERIFY(std::abs(*w.rho + 1.0) < 1e-12);
    }

    // A non-silent but constant DC pair has no qualifying AC variance.
    std::vector<double> constantLeft(20'000, 0.5);
    std::vector<double> constantRight(20'000, -0.5);
    auto dcSource = make_buffer(
        format(rgsml::audio::ChannelLayout::STEREO_LR),
        0, constantLeft, constantRight);
    QVERIFY(dcSource);
    auto dcReq = rgsml::render::RenderRequest::create(
        dcSource.value()->view(), frame_range(0, 20'000), chain,
        {binding}, frame_count(1024));
    QVERIFY(dcReq);
    auto dc = rgsml::render::render_preview(
        *dcReq.value(), *registry.value());
    QVERIFY(dc);
    const auto& dcCorr = dc.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(dcCorr.correlation_status, StereoMsCorrelationStatus::COMPLETE);
    QCOMPARE(dcCorr.correlation_windows.size(), std::size_t{1});
    QCOMPARE(dcCorr.correlation_windows.front().validity,
             StereoMsCorrelationWindowValidity::UNDEFINED_LOW_AC);
    QVERIFY(!dcCorr.correlation_windows.front().rho.has_value());

    // Source windows shorter than 400 ms are not silently padded.
    auto tooShortReq = rgsml::render::RenderRequest::create(
        input.value()->view(), frame_range(0, 1000), chain,
        {binding}, frame_count(31));
    QVERIFY(tooShortReq);
    auto tooShort = rgsml::render::render_preview(
        *tooShortReq.value(), *registry.value());
    QVERIFY(tooShort);
    const auto& shortCorr = tooShort.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(shortCorr.correlation_status, StereoMsCorrelationStatus::UNAVAILABLE);
    QVERIFY(shortCorr.correlation_windows.empty());
}


void RenderPreviewTest::stereoMsSideLowRealBranchRmsGrid100()
{
    using rgsml::dsp::MonoBassMode;
    using rgsml::render::StereoMsSideLowStatus;

    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto chain = empty_chain(*registry.value());
    const auto id = make_id("24600000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(id, "rgsml.dsp.stereo-ms", 0));
    constexpr std::size_t n = 60'000;
    std::vector<double> left(n), right(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / 48'000.0;
        left[i] = 0.4 * std::sin(2.0 * M_PI * 95.0 * t);
        right[i] = 0.25 * std::sin(2.0 * M_PI * 215.0 * t);
    }
    auto source = make_buffer(rgsml::audio::ChannelLayout::STEREO_LR,
                              0, left, right);
    QVERIFY(source);
    const auto original = bits(source.value()->view());

    for (const auto mode : {MonoBassMode::LR12, MonoBassMode::LR24}) {
        std::vector<double> reference_pre;
        for (const double low_width : {0.0, 50.0, 100.0}) {
            auto params = rgsml::dsp::StereoMsParameters::create(
                0.0, 0.0, false, mode, 150.0, low_width);
            QVERIFY(params);
            const rgsml::dsp::ModuleExecutionBinding binding{id, *params.value()};
            auto request = rgsml::render::RenderRequest::create(
                source.value()->view(), frame_range(0, 60'000),
                chain, {binding}, frame_count(31));
            auto other_partition = rgsml::render::RenderRequest::create(
                source.value()->view(), frame_range(0, 60'000),
                chain, {binding}, frame_count(4096));
            QVERIFY(request && other_partition);
            auto result = rgsml::render::render_preview(
                *request.value(), *registry.value());
            auto partitioned = rgsml::render::render_preview(
                *other_partition.value(), *registry.value());
            QVERIFY(result && partitioned);
            QCOMPARE(bits(result.value()->view()), bits(partitioned.value()->view()));
            const auto& stage =
                result.value()->stereo_ms_stage_output_sidecars().front();
            const auto& alternate =
                partitioned.value()->stereo_ms_stage_output_sidecars().front();
            QCOMPARE(stage.side_low_status, StereoMsSideLowStatus::COMPLETE);
            QCOMPARE(stage.side_low_windows.size(), std::size_t{9});
            QCOMPARE(alternate.side_low_status, StereoMsSideLowStatus::COMPLETE);
            QCOMPARE(alternate.side_low_windows.size(), stage.side_low_windows.size());
            if (low_width == 0.0) reference_pre.clear();
            for (std::size_t j = 0; j < stage.side_low_windows.size(); ++j) {
                const auto& w = stage.side_low_windows[j];
                const auto& a = alternate.side_low_windows[j];
                QCOMPARE(w.begin_frame, static_cast<std::int64_t>(j) * 4800);
                QCOMPARE(w.end_frame - w.begin_frame, std::int64_t{19200});
                QVERIFY(std::isfinite(w.rms_before) && w.rms_before > 0.0);
                QVERIFY(std::isfinite(w.rms_after) && w.rms_after >= 0.0);
                QVERIFY(std::abs(w.rms_before - a.rms_before) < 1e-14);
                QVERIFY(std::abs(w.rms_after - a.rms_after) < 1e-14);
                if (low_width == 0.0) {
                    QCOMPARE(w.rms_after, 0.0);
                    reference_pre.push_back(w.rms_before);
                } else {
                    QVERIFY(std::abs(w.rms_before - reference_pre[j]) < 1e-14);
                    QVERIFY(std::abs(w.rms_after -
                        (low_width / 100.0) * w.rms_before) < 1e-14);
                }
            }
            // With telemetry entirely disabled, the DSP PCM must be bitwise
            // identical, not merely level-equivalent.
            auto disabled_request = rgsml::render::RenderRequest::create(
                source.value()->view(), frame_range(0, 60'000),
                chain, {binding}, frame_count(257), std::size_t{0});
            QVERIFY(disabled_request);
            auto disabled = rgsml::render::render_preview(
                *disabled_request.value(), *registry.value());
            QVERIFY(disabled);
            QCOMPARE(bits(disabled.value()->view()), bits(result.value()->view()));
            const auto& no_reading =
                disabled.value()->stereo_ms_stage_output_sidecars().front();
            QCOMPARE(no_reading.side_low_status, StereoMsSideLowStatus::UNAVAILABLE);
            QVERIFY(no_reading.side_low_windows.empty());
        }
    }
    QCOMPARE(bits(source.value()->view()), original);

    // Side Low requires the REAL active crossover: OFF and Side Mute
    // never masquerade as a valid zero-energy branch measurement.
    for (const bool side_muted : {false, true}) {
        auto off = rgsml::dsp::StereoMsParameters::create(
            0.0, 0.0, side_muted,
            side_muted ? MonoBassMode::LR24 : MonoBassMode::OFF,
            150.0, 50.0);
        QVERIFY(off);
        auto request = rgsml::render::RenderRequest::create(
            source.value()->view(), frame_range(0, 60'000), chain,
            {{id, *off.value()}}, frame_count(257));
        QVERIFY(request);
        auto result = rgsml::render::render_preview(
            *request.value(), *registry.value());
        QVERIFY(result);
        const auto& observation =
            result.value()->stereo_ms_stage_output_sidecars().front();
        QCOMPARE(observation.side_low_status, StereoMsSideLowStatus::UNAVAILABLE);
        QVERIFY(observation.side_low_windows.empty());
    }

    // Partial series must be labeled PARTIAL under a constrained budget,
    // and never alter even one PCM bit.
    auto lr24 = rgsml::dsp::StereoMsParameters::create(
        0.0, 0.0, false, MonoBassMode::LR24, 150.0, 50.0);
    QVERIFY(lr24);
    const rgsml::dsp::ModuleExecutionBinding binding{id, *lr24.value()};
    constexpr std::size_t kRingBytes = 19'200 * sizeof(std::array<double, 2>);
    const std::size_t budget = 128U * 1024U + kRingBytes
                              + sizeof(rgsml::render::StereoMsSideLowWindow);
    auto tight_request = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 60'000), chain,
        {binding}, frame_count(4096), budget);
    QVERIFY(tight_request);
    auto tight = rgsml::render::render_preview(
        *tight_request.value(), *registry.value());
    QVERIFY(tight);
    const auto& budgeted = tight.value()->stereo_ms_stage_output_sidecars().front();
    QCOMPARE(budgeted.side_low_status, StereoMsSideLowStatus::PARTIAL);
    QCOMPARE(budgeted.side_low_windows.size(), std::size_t{1});
    QCOMPARE(budgeted.side_low_windows.front().begin_frame, std::int64_t{0});

    // The Side Low RMS grid is the SAME direct-rounded Grid100 as local
    // correlation, not a fixed-hop shortcut at 44,105 Hz.
    constexpr std::size_t odd_count = 50'000;
    std::vector<double> odd_left(odd_count), odd_right(odd_count);
    for (std::size_t i = 0; i < odd_count; ++i) {
        const double seconds = static_cast<double>(i) / 44'105.0;
        odd_left[i] = 0.3 * std::sin(2.0 * M_PI * 115.0 * seconds);
        odd_right[i] = 0.15 * std::cos(2.0 * M_PI * 169.0 * seconds);
    }
    auto odd_source = make_buffer(
        format(rgsml::audio::ChannelLayout::STEREO_LR, 44'105),
        0, odd_left, odd_right);
    QVERIFY(odd_source);
    auto odd_req = rgsml::render::RenderRequest::create(
        odd_source.value()->view(), frame_range(0, 50'000), chain,
        {binding}, frame_count(71));
    QVERIFY(odd_req);
    auto odd_result = rgsml::render::render_preview(
        *odd_req.value(), *registry.value());
    QVERIFY(odd_result);
    const auto& odd_low = odd_result.value()->stereo_ms_stage_output_sidecars()
        .front();
    QCOMPARE(odd_low.side_low_status, StereoMsSideLowStatus::COMPLETE);
    QVERIFY(odd_low.side_low_windows.size() >= std::size_t{5});
    const std::array<std::int64_t, 5> starts{0, 4410, 8821, 13232, 17642};
    for (std::size_t i = 0; i < starts.size(); ++i) {
        const auto& w = odd_low.side_low_windows[i];
        QCOMPARE(w.begin_frame, starts[i]);
        QCOMPARE(w.end_frame - w.begin_frame, std::int64_t{17642});
        QVERIFY(w.rms_before > 0.0);
        QVERIFY(std::abs(w.rms_after - 0.5 * w.rms_before) < 1e-14);
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
        "rgsml.dsp.dynamic-eq", 0)); // Dynamic EQ still has no production factory
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
    QVERIFY(chain.add(comp_id, "rgsml.dsp.dynamic-eq", 0)); // Dynamic EQ still has no production factory

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

    // Render request attempting to supply budget > 128 MiB (e.g. 1 GB)
    constexpr std::size_t kOneGigabyte = 1000U * 1024U * 1024U;
    auto req_overbudget = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 100), chain,
        {comp_b}, frame_count(64), kOneGigabyte);
    QVERIFY(req_overbudget);
    // RenderRequest clamps max_telemetry_bytes to 128 MiB
    QCOMPARE(req_overbudget.value()->max_telemetry_bytes().value_or(0), 128U * 1024U * 1024U);

    // Direct collector test with 1 GB passed: allocation requiring 150 MiB still fails closed at 128 MiB
    const auto dummyUuid = *rgsml::core::Uuid::parse("11111111-1111-1111-1111-111111111111").value();
    const auto dummyInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(dummyUuid).value();
    rgsml::render::CompressorTelemetryCollector hugeCol{
        0, 1000000000, 48000, rgsml::audio::ChannelLayout::STEREO_LR, rgsml::dsp::CompressorChannelLink::DUAL_MONO, dummyInstanceId, 1, kOneGigabyte
    };
    auto hugeSc = hugeCol.build_sidecar();
    QVERIFY(!hugeSc.valid);
    QCOMPARE(hugeSc.status, rgsml::render::CompressorTelemetryStatus::UNAVAILABLE);
}

void RenderPreviewTest::telemetryHistorySeamAndRealizationIdentity()
{
    auto registry = rgsml::dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);

    std::vector<double> mono_samples(1000U, 0.5);
    auto source = make_buffer(rgsml::audio::ChannelLayout::MONO_C, 0, mono_samples);
    auto chain = empty_chain(*registry.value());

    const auto comp_id = make_id("60000000-0000-0000-0000-000000000001");
    QVERIFY(chain.add(comp_id, "rgsml.dsp.compressor", 0));

    const rgsml::dsp::ModuleExecutionBinding comp_b{comp_id, *rgsml::dsp::CompressorParameters::create_default().value()};
    const rgsml::core::RealizationId testRealizationId{42};

    // Unbound render telemetry is not eligible for audible history until the
    // accepted Processed realization identity is attached.
    auto req_unbound = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1000), chain,
        {comp_b}, frame_count(64));
    QVERIFY(req_unbound);
    auto res_unbound =
        rgsml::render::render_preview(*req_unbound.value(), *registry.value());
    QVERIFY(res_unbound);
    QVERIFY(res_unbound.value()->compressor_telemetry_sidecar().has_value());
    QVERIFY(!res_unbound.value()->compressor_telemetry_sidecar()->realization_id.has_value());

    rgsml::render::CompressorTelemetryHistory unbound_history;
    unbound_history.push_sidecar(
        *res_unbound.value()->compressor_telemetry_sidecar());
    QVERIFY(!unbound_history.valid());
    QCOMPARE(
        unbound_history.status(),
        rgsml::render::CompressorTelemetryStatus::UNAVAILABLE);

    QVERIFY(
        res_unbound.value()->bind_compressor_telemetry_realization_id(
            testRealizationId));
    QCOMPARE(
        res_unbound.value()->compressor_telemetry_sidecar()->realization_id,
        std::optional{testRealizationId});
    for (const auto& lane :
         res_unbound.value()->compressor_telemetry_sidecar()->channel_lanes) {
        for (const auto& bucket : lane.buckets) {
            QCOMPARE(
                bucket.realization_id,
                std::optional{testRealizationId});
        }
    }

    // 1. Render preview carrying explicit realization identity
    auto req = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 1000), chain,
        {comp_b}, frame_count(64), std::nullopt, testRealizationId);
    QVERIFY(req);
    QCOMPARE(req.value()->realization_id(), std::optional{testRealizationId});

    auto res = rgsml::render::render_preview(*req.value(), *registry.value());
    QVERIFY(res);
    QVERIFY(res.value()->compressor_telemetry_sidecar().has_value());

    const auto& sidecar = *res.value()->compressor_telemetry_sidecar();
    QVERIFY(sidecar.valid);
    QCOMPARE(sidecar.realization_id, std::optional{testRealizationId});
    QVERIFY(!res.value()->bind_compressor_telemetry_realization_id(
        rgsml::core::RealizationId{99}));
    QCOMPARE(
        res.value()->compressor_telemetry_sidecar()->realization_id,
        std::optional{testRealizationId});
    QVERIFY(!sidecar.channel_lanes.empty());
    QVERIFY(!sidecar.channel_lanes[0].buckets.empty());

    // Verify every bucket carries the realization_id
    for (const auto& bucket : sidecar.channel_lanes[0].buckets) {
        QCOMPARE(bucket.realization_id, std::optional{testRealizationId});
        QVERIFY(bucket.frame_count > 0);
        QVERIFY(bucket.valid);
    }

    // 2. Test CompressorTelemetryHistory buffer append and FIFO retention
    rgsml::render::CompressorTelemetryHistory history{10U}; // Max 10 buckets per lane
    QCOMPARE(history.max_buckets_per_lane(), std::size_t{10});

    history.push_sidecar(sidecar);
    QVERIFY(history.valid());
    QCOMPARE(history.realization_id(), std::optional{testRealizationId});
    QCOMPARE(history.chain_revision(), sidecar.chain_revision);
    QCOMPARE(history.module_instance_id(), std::optional{comp_id});

    const auto bucket_cnt_initial = sidecar.channel_lanes[0].buckets.size();
    const auto expected_in_history = std::min(bucket_cnt_initial, std::size_t{10});
    QCOMPARE(history.bucket_count(0), expected_in_history);

    // 3. Test realization identity change -> clears old history for new realization
    const rgsml::core::RealizationId newRealizationId{99};
    auto req_new_real = rgsml::render::RenderRequest::create(
        source.value()->view(), frame_range(0, 240), chain,
        {comp_b}, frame_count(64), std::nullopt, newRealizationId);
    QVERIFY(req_new_real);
    auto res_new_real = rgsml::render::render_preview(*req_new_real.value(), *registry.value());
    QVERIFY(res_new_real);

    history.push_sidecar(*res_new_real.value()->compressor_telemetry_sidecar());
    QVERIFY(history.valid());
    QCOMPARE(history.realization_id(), std::optional{newRealizationId});
    // Buckets now belong exclusively to newRealizationId
    for (const auto& bucket : history.lane_buckets(0)) {
        QCOMPARE(bucket.realization_id, std::optional{newRealizationId});
    }

    // 4. Test UNAVAILABLE sidecar handling -> history marks status UNAVAILABLE without manufacturing zeros
    rgsml::render::CompressorTelemetrySidecar unavail_sidecar{comp_id, newRealizationId};
    unavail_sidecar.valid = false;
    unavail_sidecar.status = rgsml::render::CompressorTelemetryStatus::UNAVAILABLE;

    history.push_sidecar(unavail_sidecar);
    QVERIFY(!history.valid());
    QCOMPARE(history.status(), rgsml::render::CompressorTelemetryStatus::UNAVAILABLE);

    // 5. Test 5-second default history capacity (1000 buckets)
    rgsml::render::CompressorTelemetryHistory default_history; // Default 1000 buckets (~5s @ 200 Hz)
    QCOMPARE(default_history.max_buckets_per_lane(), std::size_t{1000});

    // Push 1200 buckets
    for (std::uint64_t i = 0; i < 1200; ++i) {
        rgsml::render::CompressorTelemetryBucket b{comp_id, testRealizationId};
        b.begin_frame = static_cast<std::int64_t>(i * 240);
        b.end_frame = static_cast<std::int64_t>((i + 1) * 240);
        b.frame_count = 240U;
        b.valid = true;
        default_history.push_bucket(0, b);
    }
    QVERIFY(default_history.valid());
    QCOMPARE(default_history.bucket_count(0), std::size_t{1000});
    // Oldest 200 buckets dropped: first bucket in history is frame 200 * 240 = 48000
    QCOMPARE(default_history.lane_buckets(0).front().begin_frame, std::int64_t{48000});
    QCOMPARE(default_history.lane_buckets(0).back().begin_frame, std::int64_t{1199 * 240});
}

void RenderPreviewTest::audibleCompressorTelemetryResolverContract()
{
    const auto dummyUuid = *rgsml::core::Uuid::parse("70000000-0000-0000-0000-000000000001").value();
    const auto compInstanceId = *rgsml::dsp::ModuleInstanceId::from_uuid(dummyUuid).value();
    const rgsml::core::RealizationId ridOld{100};
    const rgsml::core::RealizationId ridNew{200};

    // Build sidecar OLD (10 buckets of 240 frames each)
    rgsml::render::CompressorTelemetrySidecar sidecarOld{compInstanceId, ridOld};
    sidecarOld.valid = true;
    sidecarOld.status = rgsml::render::CompressorTelemetryStatus::OK;
    sidecarOld.channel_lanes.resize(1);
    for (std::uint64_t i = 0; i < 10; ++i) {
        rgsml::render::CompressorTelemetryBucket b{compInstanceId, ridOld};
        b.begin_frame = static_cast<std::int64_t>(i * 240);
        b.end_frame = static_cast<std::int64_t>((i + 1) * 240);
        b.frame_count = 240;
        b.peak_reduction_db = -3.0 - static_cast<double>(i);
        b.valid = true;
        sidecarOld.channel_lanes[0].buckets.push_back(b);
    }

    // Build sidecar NEW (10 buckets of 240 frames each)
    rgsml::render::CompressorTelemetrySidecar sidecarNew{compInstanceId, ridNew};
    sidecarNew.valid = true;
    sidecarNew.status = rgsml::render::CompressorTelemetryStatus::OK;
    sidecarNew.channel_lanes.resize(1);
    for (std::uint64_t i = 0; i < 10; ++i) {
        rgsml::render::CompressorTelemetryBucket b{compInstanceId, ridNew};
        b.begin_frame = static_cast<std::int64_t>(i * 240);
        b.end_frame = static_cast<std::int64_t>((i + 1) * 240);
        b.frame_count = 240;
        b.peak_reduction_db = -6.0 - static_cast<double>(i);
        b.valid = true;
        sidecarNew.channel_lanes[0].buckets.push_back(b);
    }

    rgsml::render::AudibleCompressorTelemetryResolver resolver;

    // 1. OLD telemetry advances while OLD audio is audible.
    resolver.register_sidecar(sidecarOld);
    rgsml::render::ResolverUpdateContext ctx;
    ctx.audition_target = rgsml::render::AuditionTarget::PROCESSED;
    ctx.playback_snapshot.state = rgsml::core::PlaybackState::PLAYING;
    ctx.playback_snapshot.traversalSerial = 1;
    ctx.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::OLD, ridOld};
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{0}; // Start traversal at frame 0

    resolver.update(ctx); // Traversal 1 initialized at frame 0
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{480}; // Position advances 0 -> 480 (buckets #0 [0..240] & #1 [240..480] crossed)
    resolver.update(ctx);

    QVERIFY(resolver.valid());
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::ACTIVE_WET);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{2});
    QCOMPARE(resolver.history().lane_buckets(0).front().realization_id, std::optional{ridOld});

    // 2. NEW telemetry does NOT advance merely because new render is ready/accepted.
    resolver.register_sidecar(sidecarNew); // NEW sidecar registered, but phase is still OLD!
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{720}; // Position advances 480 -> 720 in OLD phase (bucket #2 [480..720] crossed)
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{3});
    QCOMPARE(resolver.history().lane_buckets(0).back().realization_id, std::optional{ridOld}); // Must STILL be OLD!

    // 3. TRANSITION produces no numeric GR history.
    ctx.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::TRANSITION, std::nullopt};
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{960}; // Position advances 720 -> 960 in TRANSITION
    resolver.update(ctx);
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::TRANSITION);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{3}); // Unchanged!

    // 4. NEW telemetry advances only after Stage 3A NEW boundary.
    ctx.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, ridNew, 960}; // Handoff end boundary at frame 960
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{960}; // Position at handoff end boundary
    resolver.update(ctx);
    QCOMPARE(resolver.active_realization_id(), std::optional{ridNew});

    ctx.playback_snapshot.position = rgsml::core::FrameIndex{1200}; // Position advances 960 -> 1200 -> bucket #4 (960..1200) completes!
    resolver.update(ctx);
    QCOMPARE(resolver.history().lane_buckets(0).back().realization_id, std::optional{ridNew});

    // 5. Multi-bucket poll consumes all eligible buckets exactly once.
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{1920}; // Position moved across buckets #5 (1200..1440), #6 (1440..1680), #7 (1680..1920)
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{7}); // 3 OLD buckets + 4 NEW buckets preserved in history!

    // 6. Repeated polling without output progression produces no duplicate buckets.
    resolver.update(ctx);
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{7});

    // 7. PAUSE freezes history/cursor exactly.
    ctx.playback_snapshot.state = rgsml::core::PlaybackState::PAUSED;
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{2160};
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{7}); // Frozen on PAUSE!

    // 8. RESUME continues correctly without loss or duplication.
    ctx.playback_snapshot.state = rgsml::core::PlaybackState::PLAYING;
    resolver.update(ctx); // Position is 2160 -> now consumes bucket #8 (1920..2160)
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{8});

    // 9. STOP / EOS preserves history without advancing it.
    ctx.playback_snapshot.state = rgsml::core::PlaybackState::STOPPED;
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{2400};
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{8}); // Preserved!

    // 10. REPLAY establishes correct new traversal chronology using explicit traversalSerial.
    ctx.playback_snapshot.state = rgsml::core::PlaybackState::PLAYING;
    ctx.playback_snapshot.traversalSerial = 2; // New traversal epoch starting at frame 0!
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{0};
    ctx.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, ridNew, std::nullopt};
    resolver.update(ctx); // Traversal 2 initialized at frame 0

    ctx.playback_snapshot.position = rgsml::core::FrameIndex{240}; // Position advances to frame 240
    resolver.update(ctx);
    QCOMPARE(resolver.traversal_serial(), std::uint64_t{2});
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{1}); // History cleared for new traversal, 1 bucket consumed

    // 11. SEEK clears abandoned recent listening history and DOES NOT append bucket before seek target.
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{480};
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{2});

    ctx.playback_snapshot.seekSerial = 1; // Explicit seek to frame 240!
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{240};
    resolver.update(ctx);
    QCOMPARE(resolver.seek_serial(), std::uint64_t{1});
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{0}); // Bucket 0..240 before seek target is NOT appended!

    ctx.playback_snapshot.position = rgsml::core::FrameIndex{480}; // Position advances to frame 480 -> bucket 240..480 completes!
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{1}); // First fully eligible bucket after seek consumed!

    // 12. LOOP wrap is represented truthfully using explicit loopWrapCount.
    ctx.playback_snapshot.loopWrapCount = 1; // Explicit loop wrap to frame 240!
    ctx.playback_snapshot.position = rgsml::core::FrameIndex{240};
    resolver.update(ctx);
    QCOMPARE(resolver.loop_wrap_count(), std::uint64_t{1});
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{1}); // History preserved, loop pass starting

    ctx.playback_snapshot.position = rgsml::core::FrameIndex{480}; // Loop pass advances to frame 480 -> loop bucket 240..480 completes!
    resolver.update(ctx);
    QCOMPARE(resolver.history().bucket_count(0), std::size_t{2}); // 1st pass bucket + 2nd pass bucket = 2 buckets in 5s history!

    // 13. PREPARED target does not advance PROCESSED GR history.
    rgsml::render::ResolverUpdateContext ctxPrep = ctx;
    ctxPrep.audition_target = rgsml::render::AuditionTarget::PREPARED;
    resolver.update(ctxPrep);
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::NOT_AUDITIONED);

    // 14. GOLD target does not advance PROCESSED GR history.
    rgsml::render::ResolverUpdateContext ctxGold = ctx;
    ctxGold.audition_target = rgsml::render::AuditionTarget::GOLD;
    resolver.update(ctxGold);
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::NOT_AUDITIONED);

    // 15. BYPASS does not generate fake zero-GR history.
    const rgsml::core::RealizationId ridBypass{300};
    rgsml::render::CompressorTelemetrySidecar sidecarBypass{compInstanceId, ridBypass};
    sidecarBypass.valid = true;
    sidecarBypass.status = rgsml::render::CompressorTelemetryStatus::BYPASS;
    resolver.register_sidecar(sidecarBypass, true); // Bypass = true

    rgsml::render::ResolverUpdateContext ctxBypass = ctx;
    ctxBypass.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, ridBypass};
    const auto bucketsBeforeBypass = resolver.history().bucket_count(0);
    resolver.update(ctxBypass);
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::BYPASS);
    QCOMPARE(resolver.history().bucket_count(0), bucketsBeforeBypass); // No fake 0 dB buckets!

    // 16. Mix = 0% preserves internal wet GR while exposing ACTIVE DRY ONLY disposition.
    const rgsml::core::RealizationId ridDry{400};
    rgsml::render::CompressorTelemetrySidecar sidecarDry{compInstanceId, ridDry};
    sidecarDry.valid = true;
    sidecarDry.status = rgsml::render::CompressorTelemetryStatus::OK;
    sidecarDry.channel_lanes.resize(1);
    rgsml::render::CompressorTelemetryBucket bDry{compInstanceId, ridDry};
    bDry.begin_frame = 0; bDry.end_frame = 240; bDry.frame_count = 240; bDry.valid = true;
    sidecarDry.channel_lanes[0].buckets.push_back(bDry);

    resolver.register_sidecar(sidecarDry, false, 0.0); // mix_percent = 0.0

    rgsml::render::ResolverUpdateContext ctxDry = ctx;
    ctxDry.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, ridDry};
    ctxDry.playback_snapshot.position = rgsml::core::FrameIndex{240};
    resolver.update(ctxDry);
    QVERIFY(resolver.is_dry_only());
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::ACTIVE_DRY_ONLY);

    // 17. Stale or mismatched RealizationId fails closed to UNAVAILABLE.
    rgsml::render::ResolverUpdateContext ctxStale = ctx;
    ctxStale.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, rgsml::core::RealizationId{9999}};
    resolver.update(ctxStale);
    QVERIFY(!resolver.valid());
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::UNAVAILABLE);

    // 18. Missing/unbound telemetry fails closed to UNAVAILABLE.
    const rgsml::core::RealizationId ridUnbound{500};
    rgsml::render::CompressorTelemetrySidecar sidecarUnbound{compInstanceId, std::nullopt}; // Unbound!
    sidecarUnbound.valid = false;
    sidecarUnbound.status = rgsml::render::CompressorTelemetryStatus::UNAVAILABLE;
    resolver.register_sidecar(sidecarUnbound);

    rgsml::render::ResolverUpdateContext ctxUnbound = ctx;
    ctxUnbound.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, ridUnbound};
    resolver.update(ctxUnbound);
    QVERIFY(!resolver.valid());
    QCOMPARE(resolver.status(), rgsml::render::AudibleTelemetryStatus::UNAVAILABLE);

    // 19. OLD sidecar is retained only while needed for handoff and released afterward.
    rgsml::render::AudibleCompressorTelemetryResolver pruneResolver;
    pruneResolver.register_sidecar(sidecarOld);
    pruneResolver.register_sidecar(sidecarNew);
    QCOMPARE(pruneResolver.registered_sidecar_count(), std::size_t{2});

    rgsml::render::ResolverUpdateContext ctxHandoff = ctx;
    ctxHandoff.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::OLD, ridOld};
    pruneResolver.update(ctxHandoff);
    QCOMPARE(pruneResolver.registered_sidecar_count(), std::size_t{2});

    ctxHandoff.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, ridNew};
    pruneResolver.update(ctxHandoff);
    QCOMPARE(pruneResolver.registered_sidecar_count(), std::size_t{1}); // Only ridNew retained!

    // 20. LINKED/common and DUAL_MONO histories preserve Stage 3B1 lane semantics.
    const rgsml::core::RealizationId ridDualMono{600};
    rgsml::render::CompressorTelemetrySidecar sidecarDualMono{compInstanceId, ridDualMono};
    sidecarDualMono.valid = true;
    sidecarDualMono.status = rgsml::render::CompressorTelemetryStatus::OK;
    sidecarDualMono.channel_lanes.resize(2); // 2 DUAL_MONO lanes!
    rgsml::render::CompressorTelemetryBucket bCh0{compInstanceId, ridDualMono};
    bCh0.begin_frame = 0; bCh0.end_frame = 240; bCh0.frame_count = 240; bCh0.valid = true;
    rgsml::render::CompressorTelemetryBucket bCh1{compInstanceId, ridDualMono};
    bCh1.begin_frame = 0; bCh1.end_frame = 240; bCh1.frame_count = 240; bCh1.valid = true;
    sidecarDualMono.channel_lanes[0].buckets.push_back(bCh0);
    sidecarDualMono.channel_lanes[1].buckets.push_back(bCh1);

    rgsml::render::AudibleCompressorTelemetryResolver dualResolver;
    dualResolver.register_sidecar(sidecarDualMono);

    rgsml::render::ResolverUpdateContext ctxDual = ctx;
    ctxDual.playback_snapshot.audibleRealization = rgsml::core::AudibleRealizationState{
        rgsml::core::AudibleHandoffPhase::NEW, ridDualMono};
    ctxDual.playback_snapshot.position = rgsml::core::FrameIndex{0};
    dualResolver.update(ctxDual);

    ctxDual.playback_snapshot.position = rgsml::core::FrameIndex{240};
    dualResolver.update(ctxDual);

    QCOMPARE(dualResolver.history().num_lanes(), std::size_t{2});
    QCOMPARE(dualResolver.history().bucket_count(0), std::size_t{1});
    QCOMPARE(dualResolver.history().bucket_count(1), std::size_t{1});

    // 21. AudibleTelemetryHistory fail-closed on invalid bucket or memory limit
    rgsml::render::AudibleTelemetryHistory failHistory{1000U, 100U}; // Very small 100 byte limit
    rgsml::render::CompressorTelemetryBucket invalidBucket{compInstanceId, ridOld};
    invalidBucket.valid = false;
    failHistory.push_bucket(0, invalidBucket);
    QVERIFY(!failHistory.valid());
    QCOMPARE(failHistory.status(), rgsml::render::AudibleTelemetryStatus::UNAVAILABLE);

    // A valid bucket that exceeds the resolver's audible-history budget must
    // fail closed in the SAME update, not one polling cycle later.
    rgsml::render::AudibleCompressorTelemetryResolver budgetResolver{1000U, 1U};
    budgetResolver.register_sidecar(sidecarOld);
    rgsml::render::ResolverUpdateContext budgetCtx;
    budgetCtx.audition_target = rgsml::render::AuditionTarget::PROCESSED;
    budgetCtx.playback_snapshot.state = rgsml::core::PlaybackState::PLAYING;
    budgetCtx.playback_snapshot.traversalSerial = 1;
    budgetCtx.playback_snapshot.audibleRealization =
        rgsml::core::AudibleRealizationState{
            rgsml::core::AudibleHandoffPhase::NEW, ridOld, std::nullopt};
    budgetCtx.playback_snapshot.position = rgsml::core::FrameIndex{0};
    budgetResolver.update(budgetCtx);
    budgetCtx.playback_snapshot.position = rgsml::core::FrameIndex{240};
    budgetResolver.update(budgetCtx);
    QVERIFY(!budgetResolver.history().valid());
    QVERIFY(!budgetResolver.valid());
    QCOMPARE(
        budgetResolver.status(),
        rgsml::render::AudibleTelemetryStatus::UNAVAILABLE);
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
