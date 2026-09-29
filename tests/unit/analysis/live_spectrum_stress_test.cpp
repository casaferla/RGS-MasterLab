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
                44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, i + 1, analyzer.current_epoch());

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

        // Push frames with generation 1
        auto sig1 = rgsml::tests::oracle::generate_sine_wave(500.0, -6.0, 0.1, 44100, true);
        analyzer.push_audio_bytes(
            sig1.pcm_interleaved_f32.data(), sig1.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 1, 1);

        // Switch stream generation to 2 and invalidate
        analyzer.set_stream_generation(2);
        analyzer.invalidate_and_clear();

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // Latest snapshot must be invalid because gen 1 frames were rejected
        auto snap = analyzer.latest_snapshot();
        QVERIFY(!snap.valid || snap.stream_generation >= 2);

        analyzer.stop();
    }

    void testFormatAndRateTransitions()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        // 44.1 kHz Mono -> 48 kHz Stereo -> 96 kHz Stereo transitions
        const std::array rates = {44100U, 48000U, 88200U, 96000U};
        const std::array channels = {std::uint8_t(1), std::uint8_t(2)};

        std::uint64_t gen = 1;
        for (auto r : rates) {
            for (auto c : channels) {
                ++gen;
                analyzer.set_stream_generation(gen);
                analyzer.invalidate_and_clear();

                auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.25, r, c == 2);
                analyzer.push_audio_bytes(
                    sig.pcm_interleaved_f32.data(), sig.pcm_interleaved_f32.size() * sizeof(float),
                    r, c, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, gen, analyzer.current_epoch());

                rgsml::analysis::SpectrumSnapshot snap;
                for (int attempt = 0; attempt < 50; ++attempt) {
                    snap = analyzer.latest_snapshot();
                    if (snap.valid && snap.stream_generation == gen && snap.sample_rate_hz == r) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if (!snap.valid) {
                    std::fprintf(stderr, "FAIL DEBUG: rate=%u ch=%u targetGen=%llu snapValid=%d snapGen=%llu snapRate=%u\n",
                        r, (unsigned)c, (unsigned long long)gen, (int)snap.valid, (unsigned long long)snap.stream_generation, snap.sample_rate_hz);
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
        analyzer.set_stream_generation(1);

        // Push 0.4 seconds of audio (17,640 frames) -> exceeds overflow threshold Nwindow + 2*hop (~11,245 frames)
        auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.4, 44100, true);
        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(), sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 1, analyzer.current_epoch());

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Worker must recover and publish valid recent snapshot without crashing
        auto snap = analyzer.latest_snapshot();
        analyzer.stop();

        QVERIFY(snap.valid);
        QCOMPARE(snap.point_count, std::size_t(512));
    }
};

QTEST_MAIN(LiveSpectrumStressTest)
#include "live_spectrum_stress_test.moc"
