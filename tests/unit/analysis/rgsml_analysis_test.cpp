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

    void testHannWindowMath()
    {
        const std::size_t N = 100;
        auto h = rgsml::analysis::HannWindow::create(N);
        QCOMPARE(h.window.size(), N);
        QCOMPARE(h.window[0], 0.0);

        // Check periodic Hann symmetry and mid-point
        const double mid = 0.5 - 0.5 * std::cos((2.0 * std::numbers::pi * 50.0) / 100.0);
        QCOMPARE(h.window[50], mid);

        double expectedSumW = 0.0;
        double expectedSumW2 = 0.0;
        for (double w : h.window) {
            expectedSumW += w;
            expectedSumW2 += w * w;
        }
        QCOMPARE(h.sum_w, expectedSumW);
        QCOMPARE(h.sum_w2, expectedSumW2);
    }

    void testPsdScaling()
    {
        const std::size_t N = 8;
        std::vector<std::complex<double>> fft = {
            {10.0, 0.0}, {2.0, 0.0}, {2.0, 0.0}, {2.0, 0.0}, {4.0, 0.0}, {2.0, 0.0}, {2.0, 0.0}, {2.0, 0.0}
        };
        std::vector<double> psd(N / 2 + 1);
        const double Fs = 1000.0;
        const double sumW2 = 10.0;

        rgsml::analysis::compute_onesided_psd(fft, Fs, sumW2, psd);

        // DC (k=0): |X|^2 / (Fs * sumW2) = 100 / 10000 = 0.01
        QCOMPARE(psd[0], 0.01);
        // Positive interior (k=1): 2 * |X|^2 / (Fs * sumW2) = 2 * 4 / 10000 = 0.0008
        QCOMPARE(psd[1], 0.0008);
        // Nyquist (k=4): |X|^2 / (Fs * sumW2) = 16 / 10000 = 0.0016
        QCOMPARE(psd[4], 0.0016);
    }

    void testTriangularFrequencySmoothing()
    {
        std::vector<double> in = {1.0, 2.0, 3.0, 4.0};
        std::vector<double> out(4);

        rgsml::analysis::smooth_frequency_triangular(in, out);

        // Endpoints renormalized at 0.75:
        // out[0] = (0.50*1 + 0.25*2) / 0.75 = 1.0 / 0.75 = 1.333333...
        QCOMPARE(out[0], 1.0 / 0.75);
        // out[1] = 0.25*1 + 0.50*2 + 0.25*3 = 0.25 + 1.0 + 0.75 = 2.0
        QCOMPARE(out[1], 2.0);
        // out[2] = 0.25*2 + 0.50*3 + 0.25*4 = 0.50 + 1.50 + 1.00 = 3.0
        QCOMPARE(out[2], 3.0);
        // out[3] = (0.25*3 + 0.50*4) / 0.75 = 2.75 / 0.75 = 3.666666...
        QCOMPARE(out[3], 2.75 / 0.75);
    }

    void testTemporalSmoothingRecurrenceAndFreshRestart()
    {
        rgsml::analysis::TemporalSmoother smoother;
        smoother.configure(1874, 44100, 1);

        std::vector<double> newP = {10.0};
        std::vector<double> state = {0.0};

        // Fresh restart -> state directly initialized from newP without fading in from -90 dB
        smoother.process(newP, state, true);
        QCOMPARE(state[0], 10.0);

        // Attack recurrence: pNew (20.0) > pPrev (10.0)
        newP[0] = 20.0;
        smoother.process(newP, state, false);
        const double dt = 1874.0 / 44100.0;
        const double alphaAttack = std::exp(-dt / 0.060);
        const double expectedAttack = alphaAttack * 10.0 + (1.0 - alphaAttack) * 20.0;
        QCOMPARE(state[0], expectedAttack);

        // Release recurrence: pNew (5.0) < pPrev
        newP[0] = 5.0;
        const double prevVal = state[0];
        smoother.process(newP, state, false);
        const double alphaRelease = std::exp(-dt / 0.250);
        const double expectedRelease = alphaRelease * prevVal + (1.0 - alphaRelease) * 5.0;
        QCOMPARE(state[0], expectedRelease);
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

        rgsml::analysis::AnalysisFrame frames[2];
        std::size_t popped = ring.pop_frames(2, frames);

        QCOMPARE(popped, std::size_t(2));
        QCOMPARE(frames[0].sample_l, 0.5f);
        QCOMPARE(frames[0].sample_r, -0.5f);
        QCOMPARE(frames[0].stream_generation, std::uint64_t(1));
        QCOMPARE(frames[0].analysis_epoch, std::uint64_t(1));

        // Test PCM16 including exact -32768 -> -1.0f
        std::vector<std::int16_t> pcm16In = {-32768, 32767, 0, 16384};
        pushed = ring.push_pcm_bytes(
            pcm16In.data(), pcm16In.size() * sizeof(std::int16_t),
            48000, 2, rgsml::analysis::SampleEncoding::PCM16_LE, 2, 5);

        QCOMPARE(pushed, std::size_t(2));
        popped = ring.pop_frames(2, frames);
        QCOMPARE(popped, std::size_t(2));
        QCOMPARE(frames[0].sample_l, -1.0f);
        QCOMPARE(frames[0].sample_rate_hz, std::uint32_t(48000));
        QCOMPARE(frames[0].stream_generation, std::uint64_t(2));
        QCOMPARE(frames[0].analysis_epoch, std::uint64_t(5));
    }

    void testPartialFrameRemainderDiscardOnIdentityChange()
    {
        rgsml::analysis::SpscFrameRing ring(100);

        // Push partial frame (3 bytes of Float32 stereo which requires 8 bytes)
        std::uint8_t partialBytes[3] = {1, 2, 3};
        ring.push_pcm_bytes(
            partialBytes, 3,
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 1, 1);

        QCOMPARE(ring.available_frames(), std::size_t(0));

        // Push new generation -> remainder must be discarded clean, no mixing
        std::vector<float> f32In = {0.75f, 0.75f};
        ring.push_pcm_bytes(
            f32In.data(), f32In.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 2, 2);

        QCOMPARE(ring.available_frames(), std::size_t(1));
        rgsml::analysis::AnalysisFrame frame;
        ring.pop_frames(1, &frame);
        QCOMPARE(frame.sample_l, 0.75f);
        QCOMPARE(frame.stream_generation, std::uint64_t(2));
    }

    void testOverflowAdvancesAnalysisEpoch()
    {
        rgsml::analysis::SpscFrameRing ring(4); // tiny capacity: max 3 writable frames

        std::vector<float> f32In(20, 0.1f); // 10 frames > capacity
        std::size_t pushed = ring.push_pcm_bytes(
            f32In.data(), f32In.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32, 1, 100);

        QCOMPARE(pushed, std::size_t(3));
        rgsml::analysis::AnalysisFrame frame;
        ring.pop_frames(1, &frame);
        QVERIFY(frame.analysis_epoch > 100); // Epoch advanced due to producer capacity overflow
    }
};

QTEST_MAIN(RgsmlAnalysisTest)
#include "rgsml_analysis_test.moc"
