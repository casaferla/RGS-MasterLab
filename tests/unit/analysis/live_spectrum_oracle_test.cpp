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

        auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, 0.0, 0.5, 44100, true);

        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(),
            sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

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

    void testNonBinCenteredSineAndMultiTone()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        auto sig1 = rgsml::tests::oracle::generate_sine_wave(250.0, -6.0, 0.5, 44100, true);
        auto sig2 = rgsml::tests::oracle::generate_sine_wave(4000.0, -12.0, 0.5, 44100, true);

        std::vector<float> dualTone(sig1.pcm_interleaved_f32.size());
        for (std::size_t i = 0; i < dualTone.size(); ++i) {
            dualTone[i] = sig1.pcm_interleaved_f32[i] + sig2.pcm_interleaved_f32[i];
        }

        analyzer.push_audio_bytes(
            dualTone.data(), dualTone.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto snapshot = analyzer.latest_snapshot();
        analyzer.stop();

        QVERIFY(snapshot.valid);
        QVERIFY(snapshot.dbfs_powers.size() == 512);

        double power250 = -90.0;
        double power4000 = -90.0;
        for (std::size_t i = 0; i < snapshot.point_count; ++i) {
            const double f = snapshot.frequencies_hz[i];
            if (f >= 220.0 && f <= 280.0) {
                power250 = std::max(power250, snapshot.dbfs_powers[i]);
            }
            if (f >= 3600.0 && f <= 4400.0) {
                power4000 = std::max(power4000, snapshot.dbfs_powers[i]);
            }
        }
        QVERIFY(power250 > -12.0);
        QVERIFY(power4000 > -18.0);
    }

    void testBroadbandNoiseAndDcAndNearNyquist()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzerNoise;
        analyzerNoise.start();

        auto noiseSig = rgsml::tests::oracle::generate_broadband_noise(-12.0, 0.5, 44100, true);
        analyzerNoise.push_audio_bytes(
            noiseSig.pcm_interleaved_f32.data(), noiseSig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto noiseSnap = analyzerNoise.latest_snapshot();
        analyzerNoise.stop();

        QVERIFY(noiseSnap.valid);
        std::size_t energeticBins = 0;
        for (double p : noiseSnap.dbfs_powers) {
            if (p > -50.0) {
                ++energeticBins;
            }
        }
        QVERIFY(energeticBins > 300);

        rgsml::analysis::LiveSpectrumAnalyzer analyzerDc;
        analyzerDc.start();

        auto dcSig = rgsml::tests::oracle::generate_dc_offset(0.0, 0.5, 44100, true);
        analyzerDc.push_audio_bytes(
            dcSig.pcm_interleaved_f32.data(), dcSig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto dcSnap = analyzerDc.latest_snapshot();
        analyzerDc.stop();

        QVERIFY(dcSnap.valid);
        QVERIFY(dcSnap.dbfs_powers[0] > -40.0);

        rgsml::analysis::LiveSpectrumAnalyzer analyzerNyquist;
        analyzerNyquist.start();

        auto nyquistSig = rgsml::tests::oracle::generate_sine_wave(19500.0, -6.0, 0.5, 44100, true);
        analyzerNyquist.push_audio_bytes(
            nyquistSig.pcm_interleaved_f32.data(), nyquistSig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto nyquistSnap = analyzerNyquist.latest_snapshot();
        analyzerNyquist.stop();

        QVERIFY(nyquistSnap.valid);
        double maxHighPower = -90.0;
        for (std::size_t i = 0; i < nyquistSnap.point_count; ++i) {
            if (nyquistSnap.frequencies_hz[i] >= 18000.0) {
                maxHighPower = std::max(maxHighPower, nyquistSnap.dbfs_powers[i]);
            }
        }
        QVERIFY(maxHighPower > -15.0);
    }

    void testMonoVsIdenticalStereo()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzerMono;
        rgsml::analysis::LiveSpectrumAnalyzer analyzerStereo;
        analyzerMono.start();
        analyzerStereo.start();

        auto sigMono = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.5, 44100, false);
        auto sigStereo = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.5, 44100, true);

        analyzerMono.push_audio_bytes(
            sigMono.pcm_interleaved_f32.data(), sigMono.pcm_interleaved_f32.size() * sizeof(float),
            44100, 1, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        analyzerStereo.push_audio_bytes(
            sigStereo.pcm_interleaved_f32.data(), sigStereo.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto snapMono = analyzerMono.latest_snapshot();
        auto snapStereo = analyzerStereo.latest_snapshot();
        analyzerMono.stop();
        analyzerStereo.stop();

        QVERIFY(snapMono.valid && snapStereo.valid);

        for (std::size_t i = 0; i < snapMono.point_count; ++i) {
            QCOMPARE_LE(std::abs(snapMono.dbfs_powers[i] - snapStereo.dbfs_powers[i]), 0.5);
        }
    }

    void testAntiPhaseStereoNonCancellationAndUnequalLr()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        auto sig = rgsml::tests::oracle::generate_sine_wave(1000.0, -6.0, 0.5, 44100, true, std::numbers::pi);

        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(), sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto snapshot = analyzer.latest_snapshot();
        analyzer.stop();

        QVERIFY(snapshot.valid);
        double maxDbfs = -100.0;
        for (double p : snapshot.dbfs_powers) {
            maxDbfs = std::max(maxDbfs, p);
        }
        QVERIFY(maxDbfs > -10.0);

        rgsml::analysis::LiveSpectrumAnalyzer analyzerUnequal;
        analyzerUnequal.start();

        auto unequalSig = rgsml::tests::oracle::generate_unequal_stereo_sines(500.0, -6.0, 2000.0, -12.0, 0.5, 44100);
        analyzerUnequal.push_audio_bytes(
            unequalSig.pcm_interleaved_f32.data(), unequalSig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto unequalSnap = analyzerUnequal.latest_snapshot();
        analyzerUnequal.stop();

        QVERIFY(unequalSnap.valid);
        double power500 = -90.0;
        double power2000 = -90.0;
        for (std::size_t i = 0; i < unequalSnap.point_count; ++i) {
            const double f = unequalSnap.frequencies_hz[i];
            if (f >= 450.0 && f <= 550.0) {
                power500 = std::max(power500, unequalSnap.dbfs_powers[i]);
            }
            if (f >= 1800.0 && f <= 2200.0) {
                power2000 = std::max(power2000, unequalSnap.dbfs_powers[i]);
            }
        }
        QVERIFY(power500 > -15.0);
        QVERIFY(power2000 > -20.0);
    }

    void testSilenceFloor()
    {
        rgsml::analysis::LiveSpectrumAnalyzer analyzer;
        analyzer.start();

        auto sig = rgsml::tests::oracle::generate_silence(0.5, 44100, true);

        analyzer.push_audio_bytes(
            sig.pcm_interleaved_f32.data(),
            sig.pcm_interleaved_f32.size() * sizeof(float),
            44100, 2, rgsml::analysis::SampleEncoding::IEEE_FLOAT32);

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
