#include "live_spectrum_view_model.hpp"

namespace rgsml::app {

LiveSpectrumViewModel::LiveSpectrumViewModel(
    analysis::LiveSpectrumAnalyzer* analyzer,
    QObject* parent)
    : QObject(parent)
    , analyzer_(analyzer)
{
    pollTimer_.setInterval(33);
    connect(&pollTimer_, &QTimer::timeout, this, &LiveSpectrumViewModel::onPollTimer);
    pollTimer_.start();
}

LiveSpectrumViewModel::~LiveSpectrumViewModel() = default;

void LiveSpectrumViewModel::setSpectrumEnabled(bool enabled)
{
    if (spectrumEnabled_ != enabled) {
        spectrumEnabled_ = enabled;
        emit spectrumEnabledChanged();
    }
}

void LiveSpectrumViewModel::toggleSpectrum()
{
    setSpectrumEnabled(!spectrumEnabled_);
}

QVariantList LiveSpectrumViewModel::spectrumPoints() const
{
    return pointsList_;
}

void LiveSpectrumViewModel::onPollTimer()
{
    if (!analyzer_) {
        return;
    }

    auto snapshot = analyzer_->latest_snapshot();
    if (!snapshot.valid) {
        if (hasValidSpectrum_) {
            hasValidSpectrum_ = false;
            pointsList_.clear();
            emit spectrumPointsChanged();
        }
        return;
    }

    if (snapshot.stream_generation < lastStreamGeneration_) {
        return;
    }
    if (snapshot.stream_generation == lastStreamGeneration_) {
        if (snapshot.analysis_epoch < lastAnalysisEpoch_) {
            return;
        }
        if (snapshot.analysis_epoch == lastAnalysisEpoch_ && snapshot.sequence_number <= lastSequenceNumber_) {
            return;
        }
    }

    lastStreamGeneration_ = snapshot.stream_generation;
    lastAnalysisEpoch_ = snapshot.analysis_epoch;
    lastSequenceNumber_ = snapshot.sequence_number;
    hasValidSpectrum_ = true;

    QVariantList newPoints;
    const std::size_t N = snapshot.point_count;
    newPoints.reserve(static_cast<qsizetype>(N));

    for (std::size_t i = 0; i < N; ++i) {
        newPoints.append(QPointF(snapshot.frequencies_hz[i], snapshot.dbfs_powers[i]));
    }

    pointsList_ = std::move(newPoints);
    emit spectrumPointsChanged();
}

}  // namespace rgsml::app
