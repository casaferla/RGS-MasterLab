#include <rgsml/analysis/fft.hpp>
#include <rgsml/analysis/hann.hpp>
#include <rgsml/analysis/log_binning.hpp>
#include <rgsml/analysis/psd.hpp>
#include <rgsml/analysis/smoothing.hpp>
#include <rgsml/analysis/spectrum_config.hpp>
#include <rgsml/analysis/spsc_ring.hpp>

#include <QTest>

#include <cmath>
#include <complex>
#include <numbers>

class RgsmlAnalysisTest : public QObject {
    Q_OBJECT

private slots:
    void testStandardRateConfigs()
    {
        auto cfg44 = rgsml::analysis::SpectrumConfig::compute(44100, 2);
        QCOMPARE(cfg44.window_size, std::size_t(7497));
        QCOMPARE(cfg44.fft_size, std::size_t(8192));
        QCOMPARE(cfg44.hop_size, std::size_t(1874));

        auto cfg48 = rgsml::analysis::SpectrumConfig::compute(48000, 2);
        QCOMPARE(cfg48.window_size, std::size_t(8160));
        QCOMPARE(cfg48.fft_size, std::size_t(8192));
        QCOMPARE(cfg48.hop_size, std::size_t(2040));

        auto cfg88 = rgsml::analysis::SpectrumConfig::compute(88200, 2);
        QCOMPARE(cfg88.window_size, std::size_t(14994));
        QCOMPARE(cfg88.fft_size, std::size_t(16384));
        QCOMPARE(cfg88.hop_size, std::size_t(3748));

        auto cfg96 = rgsml::analysis::SpectrumConfig::compute(96000, 2);
        QCOMPARE(cfg96.window_size, std::size_t(16320));
        QCOMPARE(cfg96.fft_size, std::size_t(16384));
        QCOMPARE(cfg96.hop_size, std::size_t(4080));
    }

    void testHannWindow()
    {
        auto h = rgsml::analysis::HannWindow::create(100);
        QCOMPARE(h.window.size(), std::size_t(100));
        QCOMPARE(h.window[0], 0.0);
        QVERIFY(h.sum_w > 0.0);
        QVERIFY(h.sum_w2 > 0.0);
    }

    void testMonoVsIdenticalStereo()
    {
        const std::size_t N = 1024;
        std::vector<std::complex<double>> fftL(N, {10.0, 0.0});
        std::vector<std::complex<double>> fftR(N, {10.0, 0.0});
        std::vector<double> outMono(N / 2 + 1);
        std::vector<double> outStereo(N / 2 + 1);

        rgsml::analysis::compute_live_display_power(fftL, {}, 100.0, outMono);
        rgsml::analysis::compute_live_display_power(fftL, fftR, 100.0, outStereo);

        QCOMPARE(outMono[10], outStereo[10]);
    }

    void testAntiPhaseStereoNonCancellation()
    {
        const std::size_t N = 1024;
        std::vector<std::complex<double>> fftL(N, {10.0, 0.0});
        std::vector<std::complex<double>> fftR(N, {-10.0, 0.0});
        std::vector<double> outStereo(N / 2 + 1);

        rgsml::analysis::compute_live_display_power(fftL, fftR, 100.0, outStereo);

        QVERIFY(outStereo[10] > 0.0);
    }

    void testLogGridEndpoints()
    {
        auto grid44 = rgsml::analysis::LogGrid::create(44100, 512);
        QCOMPARE(grid44.point_count, std::size_t(512));
        QCOMPARE(grid44.node_centers.size(), std::size_t(512));
        QCOMPARE(grid44.node_centers[0], 20.0);
        QCOMPARE(grid44.node_centers.back(), 19845.0);

        for (std::size_t i = 1; i < 512; ++i) {
            QVERIFY(grid44.node_centers[i] > grid44.node_centers[i - 1]);
        }
    }

    void testSpscRingFloat32AndPcm16()
    {
        rgsml::analysis::SpscFrameRing ring(100);
        std::vector<float> f32In = {0.5f, -0.5f, 0.25f, -0.25f};
        std::size_t pushed = ring.push_pcm_bytes(
            f32In.data(), f32In.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 1, 1);

        QCOMPARE(pushed, std::size_t(2));
        QCOMPARE(ring.available_frames(), std::size_t(2));

        float outFloat[4];
        rgsml::analysis::SpscFrameRing::IngressMeta meta;
        std::size_t popped = ring.pop_frames_to_float(2, outFloat, meta);

        QCOMPARE(popped, std::size_t(2));
        QCOMPARE(outFloat[0], 0.5f);
        QCOMPARE(outFloat[1], -0.5f);
    }
};

QTEST_MAIN(RgsmlAnalysisTest)
#include "rgsml_analysis_test.moc"
