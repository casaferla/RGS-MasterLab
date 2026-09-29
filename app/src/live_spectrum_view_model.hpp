#ifndef RGSML_APP_LIVE_SPECTRUM_VIEW_MODEL_HPP
#define RGSML_APP_LIVE_SPECTRUM_VIEW_MODEL_HPP

#include <rgsml/analysis/live_spectrum_analyzer.hpp>

#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QVariantList>

namespace rgsml::app {

class LiveSpectrumViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool spectrumEnabled READ spectrumEnabled WRITE setSpectrumEnabled NOTIFY spectrumEnabledChanged)
    Q_PROPERTY(QVariantList spectrumPoints READ spectrumPoints NOTIFY spectrumPointsChanged)
    Q_PROPERTY(bool hasValidSpectrum READ hasValidSpectrum NOTIFY spectrumPointsChanged)

public:
    explicit LiveSpectrumViewModel(
        analysis::LiveSpectrumAnalyzer* analyzer,
        QObject* parent = nullptr);
    ~LiveSpectrumViewModel() override;

    [[nodiscard]] bool spectrumEnabled() const noexcept { return spectrumEnabled_; }
    void setSpectrumEnabled(bool enabled);

    [[nodiscard]] QVariantList spectrumPoints() const;
    [[nodiscard]] bool hasValidSpectrum() const noexcept { return hasValidSpectrum_; }

    Q_INVOKABLE void toggleSpectrum();

signals:
    void spectrumEnabledChanged();
    void spectrumPointsChanged();

private:
    void onPollTimer();

    analysis::LiveSpectrumAnalyzer* analyzer_{nullptr};
    QTimer pollTimer_;

    bool spectrumEnabled_{true};
    bool hasValidSpectrum_{false};
    std::uint64_t lastStreamGeneration_{0};
    std::uint64_t lastAnalysisEpoch_{0};
    std::uint64_t lastSequenceNumber_{0};

    QVariantList pointsList_;
};

}  // namespace rgsml::app

#endif  // RGSML_APP_LIVE_SPECTRUM_VIEW_MODEL_HPP
