#pragma once

#include "terminal_consumed_output_binding.hpp"

#include <QObject>
#include <QVariantList>
#include <QString>
#include <functional>
#include <optional>

namespace rgsml::app {

// Read-only M17 UI boundary. There is no Q_INVOKABLE that can assert
// verification. Production MUST provide authenticated consumed-output
// evidence through observe_private_terminal_consumed_output(), backed by
// serialized Windows worker prefix reads. Synthetic observer data is
// permitted only in the corresponding test fixture.
// Even a verified private observation does NOT qualify DAC output or WAV.
class TerminalPrivateGrViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool verified READ verified NOTIFY changed)
    Q_PROPERTY(QVariantList verifiedSeries READ verified_series NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    using Observation =
        std::function<core::Result<TerminalPrivateGrSeries>()>;
    // Production uses the authoritative two-read Windows owner/clock
    // witness directly. Only the expensive gain projection is memoized;
    // an unchanged timestamp NEVER bypasses independent device evidence.
    using EvidenceObservation =
        std::function<core::Result<TerminalConsumedOutputEvidence>()>;

    explicit TerminalPrivateGrViewModel(
        Observation observe,QObject* parent=nullptr);
    explicit TerminalPrivateGrViewModel(
        EvidenceObservation observe,QObject* parent=nullptr);

    [[nodiscard]] bool verified() const noexcept { return verified_; }
    [[nodiscard]] QVariantList verified_series() const { return series_; }
    [[nodiscard]] QString status() const { return status_; }

public slots:
    void refresh();
    void invalidate();
signals:
    void changed();
private:
    struct ProvenEpoch {
        core::RealizationId realizationId;
        std::uint64_t playbackGeneration;
        std::int64_t frameBegin;
        std::int64_t frameEndExclusive;
        std::int64_t watermarkExclusive;
        std::uint64_t loopWrapCount;
        std::uint64_t seekSerial;
        std::uint64_t traversalSerial;
    };
    // A failed device read immediately hides the GR plot, but a bounded
    // previously VERIFIED trace can bridge to the next verified same-owner
    // traversal. Explicit product invalidation discards it permanently.
    void suspend_unverified();
    struct EvidenceKey final {
        std::shared_ptr<const TerminalCandidateGate::StagedCandidate> owner;
        core::RealizationId realizationId;
        std::uint64_t generation,wrap,seek,traversal;
        std::int64_t watermark,begin,end;
        [[nodiscard]] bool operator==(const EvidenceKey&) const = default;
    };
    Observation observe_;
    EvidenceObservation observeEvidence_;
    std::optional<EvidenceKey> lastEvidence_;
    bool verified_{false};
    QVariantList series_;
    QString status_{QStringLiteral("UNAVAILABLE_NO_VERIFIED_GR")};
    std::optional<core::RealizationId> realization_;
    std::uint64_t generation_{0};
    std::optional<ProvenEpoch> lastProven_;
    QVariantList lastProvenPoints_;
    QVariantList historicalPrefix_;
    bool historicalLoopMarker_{false};
};

} // namespace rgsml::app
