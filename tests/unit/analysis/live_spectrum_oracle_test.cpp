#include <rgsml/analysis/live_spectrum_analyzer.hpp>
#include "../../oracles/live_spectrum/live_spectrum_oracle.hpp"

#include <QTest>

#include <chrono>
#include <thread>

class LiveSpectrumOracleTest : public QObject {
    Q_OBJECT

private slots:
    void testBinCenteredSinePower()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();
        analyzer.set_stream_generation(1);

        auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, 0.0, 0.5, 44100, true);

        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(),
            sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 1, 1);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto snapshot = analyzer.latest_snapshot();
        analyzer.stop();

        QVERIFY(snapshot.valid);
        QCOMPARE(snapshot.point_count, std::size_t(512));
        QCOMPARE(snapshot.dbfs_powers.size(), std::size_t(512));

        double maxDbfs = -100.0;
        double maxFreq = 0.0;
        for (std::size_t i = 0; i < snapshot.point_count; ++i) {
            if (snapshot.dbfs_powers[i] > maxDbfs) {
                maxDbfs = snapshot.dbfs_powers[i];
                maxFreq = snapshot.frequencies_hz[i];
            }
        }

        QVERIFY(maxDbfs >= -3.0 && maxDbfs <= 1.0);
        QVERIFY(maxFreq >= 900.0 && maxFreq <= 1100.0);
    }

    void testSilenceFloor()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();
        analyzer.set_stream_generation(1);

        auto sig = rgsml::tests::oracle::generate_silence(0.5, 44100, true);

        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(),
            sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 1, 1);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto snapshot = analyzer.latest_snapshot();
        analyzer.stop();

        QVERIFY(snapshot.valid);
        for (double p : snapshot.dbfs_powers) {
            QVERIFY(p <= -90.0);
        }
    }
};

QTEST_MAIN(LiveSpectrumOracleTest)
#include "live_spectrum_oracle_test.moc"
