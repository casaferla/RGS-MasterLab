#include "audition_source_selector.hpp"

#include "playback_transport_view_model.hpp"

#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/audio/wav_reader.hpp>
#include <rgsml/dsp/module_execution_binding.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/platform/windows/windows_resource_identity.hpp>
#include <rgsml/platform/windows/windows_resource_reader.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace rgsml::app {
namespace {

constexpr std::int64_t kRealizationBlockFrames = 4096;

[[nodiscard]] QString target_label(AuditionTarget target)
{
    switch (target) {
    case AuditionTarget::PREPARED:
        return QStringLiteral("PREPARED");
    case AuditionTarget::PROCESSED:
        return QStringLiteral("PROCESSED");
    case AuditionTarget::GOLD:
        return QStringLiteral("GOLD");
    }
    return QStringLiteral("NONE");
}

[[nodiscard]] core::Status unavailable(const char* message)
{
    return core::Status::failure(core::Error{
        core::ErrorCode::InvalidState, message});
}

}  // namespace

AuditionSourceSelector::AuditionSourceSelector(
    PlaybackTransportViewModel* playback,
    QObject* parent)
    : QObject(parent)
    , playback_(playback)
{
}

AuditionSourceSelector::~AuditionSourceSelector() noexcept
{
    if (playback_ != nullptr) {
        static_cast<void>(playback_->stop_and_clear());
    }
}

bool AuditionSourceSelector::prepared_available() const noexcept
{
    return prepared_ != nullptr;
}

bool AuditionSourceSelector::processed_available() const noexcept
{
    return processed_ != nullptr;
}

bool AuditionSourceSelector::gold_available() const noexcept
{
    return gold_.has_value();
}

QString AuditionSourceSelector::active_target_label() const
{
    return activeTarget_ ? target_label(*activeTarget_) : QStringLiteral("NONE");
}

bool AuditionSourceSelector::gold_active() const noexcept
{
    return activeTarget_ == AuditionTarget::GOLD;
}

bool AuditionSourceSelector::source_playhead_visible() const noexcept
{
    return !gold_active();
}

QString AuditionSourceSelector::status_text() const
{
    return statusText_;
}

std::optional<AuditionTarget> AuditionSourceSelector::active_target() const noexcept
{
    return activeTarget_;
}

core::FrameIndex AuditionSourceSelector::source_derived_cue() const noexcept
{
    return sourceDerivedCue_;
}

core::FrameIndex AuditionSourceSelector::gold_cue() const noexcept
{
    return goldCue_;
}

std::shared_ptr<const render::RenderResult>
AuditionSourceSelector::prepared_realization_snapshot() const noexcept
{
    return prepared_;
}

std::shared_ptr<const render::RenderResult>
AuditionSourceSelector::processed_realization_snapshot() const noexcept
{
    return processed_;
}

void AuditionSourceSelector::set_source_loop_provider(SourceLoopProvider provider)
{
    sourceLoopProvider_ = std::move(provider);
}

core::Status AuditionSourceSelector::source_committed(
    const core::ResourceReference& source)
{
    auto cleared = playback_->stop_and_clear();
    if (!cleared) {
        fail_closed(*cleared.error());
        return cleared;
    }
    source_ = source;
    sourceDerivedCue_ = core::FrameIndex{0};
    prepared_.reset();
    processed_.reset();
    processedRealizationId_.reset();
    activeTarget_.reset();

    if (gold_) {
        if (source.same_resource_identity(*gold_)) {
            gold_.reset();
            goldRate_.reset();
            goldFrames_.reset();
        } else {
            auto same = platform::windows::same_underlying_local_file(source, *gold_);
            if (!same || *same.value()) {
                gold_.reset();
                goldRate_.reset();
                goldFrames_.reset();
            }
        }
    }

    auto materialized = materialize_prepared(source);
    if (!materialized) {
        fail_closed(*materialized.error());
        return materialized;
    }
    statusText_.clear();
    emit changed();
    return core::Status::success();
}

core::Status AuditionSourceSelector::set_prepared_realization(
    render::RenderResult realization)
{
    if (activeTarget_ == AuditionTarget::PREPARED) {
        auto cleared = playback_->stop_and_clear();
        if (!cleared) {
            return cleared;
        }
        activeTarget_.reset();
    }
    prepared_ = std::make_shared<const render::RenderResult>(std::move(realization));
    emit changed();
    return core::Status::success();
}

core::Status AuditionSourceSelector::set_processed_realization(
    render::RenderResult realization)
{
    if (nextProcessedRealizationIdValue_
        == std::numeric_limits<std::uint64_t>::max()) {
        return core::Status::failure(core::Error{
            core::ErrorCode::IntegerOverflow,
            "Processed realization identity sequence is exhausted."});
    }

    const core::RealizationId candidateRealizationId{
        nextProcessedRealizationIdValue_};
    auto identityBound =
        realization.bind_compressor_telemetry_realization_id(
            candidateRealizationId);
    if (!identityBound) {
        return identityBound;
    }
    // Bind stage-capture provenance to the SAME candidate before publishing
    // the Processed realization or handing PCM off to playback.
    auto msIdentityBound =
        realization.bind_stereo_ms_stage_output_realization_id(
            candidateRealizationId);
    if (!msIdentityBound) {
        return msIdentityBound;
    }
    auto candidate = std::make_shared<const render::RenderResult>(
        std::move(realization));

    if (activeTarget_ == AuditionTarget::PROCESSED) {
        const auto range = candidate->render_window();
        auto snapshot = playback_->playback_snapshot();
        if (snapshot) {
            sourceDerivedCue_ = snapshot.value()->position;
        }
        if (sourceDerivedCue_.value() == range.end().value()) {
            sourceDerivedCue_ = range.begin();
        }
        if (sourceDerivedCue_ < range.begin() || sourceDerivedCue_ > range.end()) {
            publish_error(QStringLiteral("Source cue is outside the available realization range."));
            return unavailable("Source cue is outside the available realization range.");
        }

        playback_->set_source_derived_active(true);
        auto handedOff = playback_->handoff_pcm(
            candidate->view(), candidate, candidateRealizationId);
        if (!handedOff) {
            publish_error(QString::fromStdString(handedOff.error()->message()));
            return handedOff;
        }

        const auto loop = sourceLoopProvider_ ? sourceLoopProvider_() : std::nullopt;
        if (!loop || (loop->begin() >= range.begin() && loop->end() <= range.end())) {
            static_cast<void>(playback_->set_loop_source_range(loop));
        } else {
            static_cast<void>(playback_->set_loop_source_range(std::nullopt));
        }

        processed_ = candidate;
        processedRealizationId_ = candidateRealizationId;
        ++nextProcessedRealizationIdValue_;
        statusText_.clear();
        emit changed();
        return core::Status::success();
    }

    processed_ = candidate;
    processedRealizationId_ = candidateRealizationId;
    ++nextProcessedRealizationIdValue_;
    emit changed();
    return core::Status::success();
}

core::Status AuditionSourceSelector::set_gold(
    core::ResourceReference reference,
    core::SampleRate sampleRate,
    core::FrameCount frameCount)
{
    if (source_) {
        if (source_->same_resource_identity(reference)) {
            return unavailable("Gold must be a file distinct from Source.");
        }
        auto same = platform::windows::same_underlying_local_file(*source_, reference);
        if (!same) {
            return core::Status::failure(*same.error());
        }
        if (*same.value()) {
            return unavailable("Gold must not be an alias of the Source file.");
        }
    }
    if (activeTarget_ == AuditionTarget::GOLD) {
        auto cleared = playback_->stop_and_clear();
        if (!cleared) {
            return cleared;
        }
        activeTarget_.reset();
    }
    gold_ = std::move(reference);
    goldRate_ = sampleRate;
    goldFrames_ = frameCount;
    goldCue_ = core::FrameIndex{0};
    statusText_.clear();
    emit changed();
    return core::Status::success();
}

core::Status AuditionSourceSelector::clear_gold()
{
    const bool wasGold = activeTarget_ == AuditionTarget::GOLD;
    if (wasGold) {
        auto stored = store_active_cue();
        if (!stored) {
            return stored;
        }
        auto cleared = playback_->stop_and_clear();
        if (!cleared) {
            return cleared;
        }
        activeTarget_.reset();
    }
    gold_.reset();
    goldRate_.reset();
    goldFrames_.reset();
    if (!wasGold) {
        emit changed();
        return core::Status::success();
    }
    if (prepared_) {
        return switch_to(AuditionTarget::PREPARED);
    }
    if (processed_) {
        return switch_to(AuditionTarget::PROCESSED);
    }
    playback_->set_source_derived_active(false);
    emit changed();
    return core::Status::success();
}

core::Status AuditionSourceSelector::switch_to(AuditionTarget target)
{
    if ((target == AuditionTarget::PREPARED && !prepared_)
        || (target == AuditionTarget::PROCESSED && !processed_)
        || (target == AuditionTarget::GOLD && !gold_)) {
        return unavailable("The requested audition target has no authoritative realization.");
    }
    if (activeTarget_ == target) {
        return core::Status::success();
    }

    bool wasPlaying = false;
    if (activeTarget_) {
        auto snapshot = playback_->playback_snapshot();
        if (snapshot && snapshot.value()->state == core::PlaybackState::PLAYING) {
            wasPlaying = true;
        }
    }

    auto stored = store_active_cue();
    if (!stored) {
        fail_closed(*stored.error());
        return stored;
    }
    auto cleared = playback_->stop_and_clear();
    if (!cleared) {
        fail_closed(*cleared.error());
        return cleared;
    }
    activeTarget_.reset();

    core::Status preparedStatus = core::Status::success();
    if (target == AuditionTarget::GOLD) {
        playback_->set_source_derived_active(false);
        preparedStatus = playback_->prepare_file(*gold_, goldRate_->value());
        if (preparedStatus && goldCue_.value() > 0) {
            preparedStatus = playback_->seek_target_frame(goldCue_);
        }
    } else {
        playback_->set_source_derived_active(true);
        const auto& real =
            (target == AuditionTarget::PREPARED ? prepared_ : processed_);
        const auto realizationId =
            target == AuditionTarget::PROCESSED
            ? processedRealizationId_
            : std::nullopt;
        preparedStatus = prepare_realization(
            *real, real, realizationId);
    }
    if (!preparedStatus) {
        fail_closed(*preparedStatus.error());
        return preparedStatus;
    }

    if (wasPlaying) {
        playback_->playOrResume();
        auto newSnapshot = playback_->playback_snapshot();
        if (newSnapshot && newSnapshot.value()->state != core::PlaybackState::PLAYING) {
            fail_closed(core::Error{
                core::ErrorCode::InvalidState,
                "Failed to resume playback after audition target switch."});
            return unavailable("Failed to resume playback after audition target switch.");
        }
    }

    activeTarget_ = target;
    statusText_.clear();
    emit changed();
    return core::Status::success();
}

void AuditionSourceSelector::selectPrepared()
{
    auto result = switch_to(AuditionTarget::PREPARED);
    if (!result) publish_error(QString::fromStdString(result.error()->message()));
}

void AuditionSourceSelector::selectProcessed()
{
    auto result = switch_to(AuditionTarget::PROCESSED);
    if (!result) publish_error(QString::fromStdString(result.error()->message()));
}

void AuditionSourceSelector::selectGold()
{
    auto result = switch_to(AuditionTarget::GOLD);
    if (!result) publish_error(QString::fromStdString(result.error()->message()));
}

core::Status AuditionSourceSelector::store_active_cue()
{
    if (!activeTarget_) {
        return core::Status::success();
    }
    auto snapshot = playback_->playback_snapshot();
    if (!snapshot) {
        return core::Status::failure(*snapshot.error());
    }
    if (*activeTarget_ == AuditionTarget::GOLD) {
        goldCue_ = snapshot.value()->position;
    } else {
        sourceDerivedCue_ = snapshot.value()->position;
    }
    return core::Status::success();
}

core::Status AuditionSourceSelector::prepare_realization(
    const render::RenderResult& realization,
    std::shared_ptr<const void> lifetime,
    std::optional<core::RealizationId> realizationId)
{
    const auto range = realization.render_window();
    if (sourceDerivedCue_.value() == range.end().value()) {
        sourceDerivedCue_ = range.begin();
    }
    if (sourceDerivedCue_ < range.begin() || sourceDerivedCue_ > range.end()) {
        return unavailable("Source cue is outside the available realization range.");
    }
    auto prepared = playback_->prepare_pcm(
        realization.view(), std::move(lifetime), realizationId);
    if (!prepared) {
        return prepared;
    }
    if (sourceDerivedCue_.value() != range.begin().value()) {
        auto sought = playback_->seek_target_frame(sourceDerivedCue_);
        if (!sought) {
            return sought;
        }
    }
    const auto loop = sourceLoopProvider_ ? sourceLoopProvider_() : std::nullopt;
    if (loop && loop->begin() >= range.begin() && loop->end() <= range.end()) {
        return playback_->set_loop_source_range(loop);
    }
    return playback_->set_loop_source_range(std::nullopt);
}

core::Status AuditionSourceSelector::materialize_prepared(
    const core::ResourceReference& source)
{
    auto resource = platform::windows::WindowsResourceReader::open_read_only(source);
    if (!resource) return core::Status::failure(*resource.error());
    auto reader = audio::WavReader::open(std::move(*resource.value()));
    if (!reader) return core::Status::failure(*reader.error());
    const auto info = (*reader.value())->info();
    auto decoded = audio::AudioBuffer::create(
        info.audio_format(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0}, info.frame_count());
    if (!decoded) return core::Status::failure(*decoded.error());

    std::int64_t completed = 0;
    while (completed < info.frame_count().value()) {
        const auto countValue = std::min(
            kRealizationBlockFrames, info.frame_count().value() - completed);
        auto count = core::FrameCount::create(countValue);
        if (!count) return core::Status::failure(*count.error());
        auto destination = decoded.value()->mutable_view().subview(
            core::FrameIndex{completed}, *count.value());
        if (!destination) return core::Status::failure(*destination.error());
        auto read = (*reader.value())->read_frames(
            core::FrameIndex{completed}, *destination.value());
        if (!read || read.value()->value() != countValue) {
            return read
                ? unavailable("Prepared realization decode returned a short block.")
                : core::Status::failure(*read.error());
        }
        completed += countValue;
    }
    auto closed = (*reader.value())->close();
    if (!closed) return closed;

    auto registry = dsp::ModuleRegistry::create_dsp_package_v1();
    if (!registry) return core::Status::failure(*registry.error());
    auto chain = dsp::ProcessingChain::create(
        *registry.value(),
        dsp::ProcessingChainContext{
            dsp::ProcessingStage::RESTORE_PREP, dsp::ChainSegment::REPAIR});
    if (!chain) return core::Status::failure(*chain.error());
    auto request = render::RenderRequest::create(
        decoded.value()->view(), decoded.value()->view().absolute_range(),
        *chain.value(), std::vector<dsp::ModuleExecutionBinding>{},
        *core::FrameCount::create(kRealizationBlockFrames).value());
    if (!request) return core::Status::failure(*request.error());
    auto rendered = render::render_preview(*request.value(), *registry.value());
    if (!rendered) return core::Status::failure(*rendered.error());
    prepared_ = std::make_shared<const render::RenderResult>(std::move(*rendered.value()));
    emit changed();
    return core::Status::success();
}

void AuditionSourceSelector::fail_closed(const core::Error& error)
{
    static_cast<void>(playback_->stop_and_clear());
    activeTarget_.reset();
    publish_error(QString::fromStdString(error.message()));
}

void AuditionSourceSelector::publish_error(QString message)
{
    statusText_ = std::move(message);
    emit changed();
}

}  // namespace rgsml::app
