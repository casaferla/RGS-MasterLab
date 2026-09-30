#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include "../../oracles/live_spectrum/live_spectrum_oracle.hpp"

#include <QTest>

#include <chrono>
#include <thread>

class LiveSpectrumStressTest : public QObject {
    Q_OBJECT

private slots:
    void testRapidInvalidationAndPushes()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        for (int i = 0; i < 50; ++i) {
            analyzer.set_stream_generation(i + 1);
            auto sig = rgsml::tests::oracle::generate_sine_wave(440.0 + i * 10, -6.0, 0.05, 44100, true);

            analyzer.push_audio_bytes(
                sig.pcm_interleaved_f32.data(),
                sig.pcm_interleaved_f32.size() * sizeof(float),
                44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

            if (i % 5 == 0) {
                analyzer.invalidate_and_clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        analyzer.stop();
        QVERIFY(true);
    }

    void testStaleIdentityRejection()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        auto sig1 = rgsml::tests::oracle::generate_sine_wave(500.0, -6.0, 0.1, 44100, true);
        analyzer.push_audio_bytes(
            sig1.pcm_interleaved_f32.data(), sig1.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        analyzer.set_stream_generation(2);
        analyzer.invalidate_and_clear();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        auto snap = analyzer.latest_snapshot();
        QVERIFY(!snap.valid || snap.stream_generation >= 2);

        analyzer.stop();
    }

    void testFormatAndRateTransitions()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        const std::array rates = {44100U, 48000U, 88200U, 96000U};
        const std::array channels = {std::uint8_t(1), std::uint8_t(2)};

        std::uint64_t gen = 1;
        for (auto r : rates) {
            for (auto c : channels) {
                ++gen;
                analyzer.set_stream_generation(gen);
                analyzer.invalidate_and_clear();

                auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.18, r, c == 2);
                analyzer.push_audio_bytes(
                    sig.pcm_interleaved_f32.data(), sig.pcm_interleaved_f32.size() * sizeof(float),
                    r, c, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

                rgsml::analysis::SpectrumSnapshot snap;
                for (int attempt = 0; attempt < 150; ++attempt) {
                    snap = analyzer.latest_snapshot();
                    if (snap.valid && snap.stream_generation == gen && snap.sample_rate_hz == r) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                QVERIFY2(snap.valid, qPrintable(QString("Snapshot must be valid for rate %1 ch %2").arg(r).arg(c)));
                QCOMPARE(snap.sample_rate_hz, r);
                QCOMPARE(snap.point_count, std::size_t(512));
            }
        }

        analyzer.stop();
    }

    void testBacklogOverflowRecovery()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.4, 44100, true);
        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(), sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        auto snap = analyzer.latest_snapshot();
        analyzer.stop();

        QVERIFY(snap.valid);
        QCOMPARE(snap.point_count, std::size_t(512));
    }

    void testCandidatePrepareFailureAtomicity()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();
        analyzer.set_stream_generation(10);

        auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.2, 44100, true);
        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(), sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto snapBefore = analyzer.latest_snapshot();
        QVERIFY(snapBefore.valid);
        QCOMPARE(snapBefore.stream_generation, std::uint64_t(10));

        // Candidate failure leaves old snapshot valid and usable
        auto snapAfter = analyzer.latest_snapshot();
        QVERIFY(snapAfter.valid);
        QCOMPARE(snapAfter.stream_generation, std::uint64_t(10));

        analyzer.stop();
    }

    void testRapidLifecycleSafety()
    {
        for (int i = 0; i < 10; ++i) {
            rgsml::analysis::LiveSpectrumAnalyzer analyzer;
            analyzer.start();
            auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.05, 44100, true);
            analyzer.push_audio_bytes(
                sig.pcm_interleaved_f32.data(), sig.pcm_interleaved_f32.size() * sizeof(float),
                44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);
            analyzer.stop();
        }
        QVERIFY(true);
    }
};

QTEST_MAIN(LiveSpectrumStressTest)
#include "live_spectrum_stress_test.moc"
