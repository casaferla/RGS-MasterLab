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
                44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, i + 1, i + 1);

            if (i % 5 == 0) {
                analyzer.invalidate_and_clear();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        analyzer.stop();
        QVERIFY(true);
    }
};

QTEST_MAIN(LiveSpectrumStressTest)
#include "live_spectrum_stress_test.moc"
