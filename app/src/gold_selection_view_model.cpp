#include "gold_selection_view_model.hpp"

#include "audition_source_selector.hpp"

#include <rgsml/platform/windows/windows_resource_reader.hpp>

#include <QFileInfo>
#include <QThread>

#include <string_view>
#include <utility>

namespace rgsml::app {

GoldSelectionViewModel::GoldSelectionViewModel(
    AuditionSourceSelector* selector,
    QObject* parent)
    : QObject(parent)
    , selector_(selector)
{
}

bool GoldSelectionViewModel::has_gold() const noexcept
{
    return selector_->gold_available();
}

QString GoldSelectionViewModel::display_name() const
{
    return displayName_;
}

QString GoldSelectionViewModel::metadata() const
{
    return metadata_;
}

QString GoldSelectionViewModel::error_message() const
{
    return errorMessage_;
}

const audio::SourceResource* GoldSelectionViewModel::gold_resource() const noexcept
{
    return gold_ ? &*gold_ : nullptr;
}

void GoldSelectionViewModel::selectGold(const QUrl& selectedFile)
{
    if (!selectedFile.isValid() || !selectedFile.isLocalFile()) {
        errorMessage_ = QStringLiteral("Choose one local Gold WAV file.");
        emit changed();
        return;
    }

    const auto path = selectedFile.toLocalFile();
    const auto name = QFileInfo{path}.fileName();
    const auto pathUtf8 = path.toUtf8();
    const auto nameUtf8 = name.toUtf8();
    auto reference = platform::windows::WindowsResourceReader::make_read_reference(
        std::string_view{pathUtf8.constData(), static_cast<std::size_t>(pathUtf8.size())},
        std::string_view{nameUtf8.constData(), static_cast<std::size_t>(nameUtf8.size())});
    if (!reference) {
        errorMessage_ = QString::fromStdString(reference.error()->message());
        emit changed();
        return;
    }
    auto reader = platform::windows::WindowsResourceReader::open_read_only(
        *reference.value());
    if (!reader) {
        errorMessage_ = QString::fromStdString(reader.error()->message());
        emit changed();
        return;
    }
    auto candidate = audio::SourceResource::probe(std::move(*reader.value()));
    if (!candidate) {
        errorMessage_ = QString::fromStdString(candidate.error()->message());
        emit changed();
        return;
    }
    const auto accepted=adopt_probed_gold(std::move(*candidate.value()));
    if(!accepted) {
        const auto message=QString::fromStdString(accepted.error()->message());
        if(errorMessage_!=message) {
            errorMessage_=message;
            emit changed();
        }
    }
}

core::Status GoldSelectionViewModel::admit_m18_gold_mutation(
    const char* operation) const
{
    if(!m18GoldMutationAdmission_)return core::Status::success();
    if(QThread::currentThread()!=thread())
        return core::Status::failure(core::Error{
            core::ErrorCode::InvalidState,
            "M18_GOLD_MUTATION_REQUIRES_QT_OWNER_THREAD"});
    try{return m18GoldMutationAdmission_(operation);}
    catch(...){return core::Status::failure(core::Error{
        core::ErrorCode::InvalidState,
        "M18_GOLD_MUTATION_ADMISSION_THROWN"});}
}

core::Status GoldSelectionViewModel::adopt_probed_gold(audio::SourceResource candidate)
{
    const auto admission=admit_m18_gold_mutation("gold_adopt_probed");
    if(!admission)return admission;
    const auto& info = candidate.wav_info();
    auto committed = selector_->set_gold(
        candidate.reference(),
        info.audio_format().sample_rate(),
        info.frame_count());
    if (!committed) {
        errorMessage_ = QString::fromStdString(committed.error()->message());
        emit changed();
        return committed;
    }
    gold_.emplace(std::move(candidate));
    displayName_ = QString::fromUtf8(
        gold_->reference().display_name().data(),
        static_cast<qsizetype>(gold_->reference().display_name().size()));
    metadata_ = QStringLiteral("%1 Hz · %2 channel(s) · %3 frames · read-only")
        .arg(info.audio_format().sample_rate().value())
        .arg(info.audio_format().channel_count())
        .arg(info.frame_count().value());
    errorMessage_.clear();
    emit changed();
    return core::Status::success();
}

void GoldSelectionViewModel::cancelGoldSelection() noexcept
{
    // Picker cancellation is a complete no-op.
}

void GoldSelectionViewModel::clearGold()
{
    const auto admission=admit_m18_gold_mutation("gold_clear");
    if(!admission) {
        errorMessage_=QString::fromStdString(admission.error()->message());
        emit changed();
        return;
    }
    auto cleared = selector_->clear_gold();
    if (!cleared) {
        errorMessage_ = QString::fromStdString(cleared.error()->message());
        emit changed();
        return;
    }
    gold_.reset();
    displayName_.clear();
    metadata_.clear();
    errorMessage_.clear();
    emit changed();
}

void GoldSelectionViewModel::sourceChanged()
{
    // Derived SOURCE invalidation: the Product must admit the SOURCE
    // mutation before it changes the selector; GOLD cannot retroactively
    // refuse that already-published outer transaction.
    if (!selector_->gold_available()) {
        gold_.reset();
        displayName_.clear();
        metadata_.clear();
        emit changed();
    }
}

}  // namespace rgsml::app
