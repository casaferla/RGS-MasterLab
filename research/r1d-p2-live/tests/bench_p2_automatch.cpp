// RGS MasterLab — R1d-P2 research-only, isolated full-song offline workload.
// NOT an L3 solver, NOT a real-time callback test, NOT a Product benchmark.
// Uses the genuine IModule P2 against deterministic synthetic stereo PCM.
// Reads no user WAV, writes no audio, measures steady-clock wall times only.
#include "research_p2_imodule.hpp"
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_descriptor.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace core = rgsml::core;
namespace audio = rgsml::audio;
namespace dsp = rgsml::dsp;
using rgsml::research::r1d_p2::ResearchP2IModule;
using rgsml::research::r1d_p2::Configuration;
using Clock = std::chrono::steady_clock;
constexpr std::size_t kBlock = 1024;
constexpr std::int64_t kOrigin = 937;
constexpr double kPi = 3.14159265358979323846264338327950288;

void require(bool good, const char* what)
{
    if (!good) throw std::runtime_error(what);
}
core::FrameCount frame_count(std::int64_t value)
{
    auto n = core::FrameCount::create(value);
    require(bool(n), "invalid frame count");
    return *n.value();
}
core::FrameRange frame_range(std::int64_t first, std::int64_t count)
{
    const auto range = core::FrameRange::create(
        core::FrameIndex{first}, core::FrameIndex{first + count});
    require(bool(range), "invalid frame range");
    return *range.value();
}
dsp::ModuleDescriptor p2_descriptor()
{
    dsp::ModuleDescriptorSpec spec{
        "rgsml.dsp.streaming-true-peak-limiter", "Streaming TP Limiter",
        {dsp::ModuleCategory::DYNAMICS}, {dsp::ProcessingStage::MASTER},
        {dsp::ChainSegment::MANUAL}, false, true,
        dsp::PlacementClass::INLINE_CHAIN, std::nullopt, {}, {}, {}, {},
        true, true, std::string{"0.1.0-research"},
        std::string{"rgsml.dsp.streaming-true-peak-limiter.params/0.1.0"}};
    auto r = dsp::ModuleDescriptor::create(std::move(spec));
    require(bool(r), "cannot create P2 research descriptor");
    return std::move(*r.value());
}
double elapsed_ms(Clock::time_point begin, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>{end - begin}.count();
}
std::uint64_t fingerprint(const audio::AudioBuffer& pcm)
{
    // Diagnostics only, not a cryptographic PCM proof.
    std::uint64_t hash = 14695981039346656037ULL;
    auto v = pcm.view();
    for (int c = 0; c < 2; ++c) {
        auto channel = v.channel(c);
        require(bool(channel), "read channel for fingerprint");
        for (double sample : *channel.value()) {
            require(std::isfinite(sample), "P2 produced nonfinite PCM");
            const auto bits = std::bit_cast<std::uint64_t>(sample);
            for (unsigned shift = 0; shift < 64; shift += 8) {
                hash ^= (bits >> shift) & 255U;
                hash *= 1099511628211ULL;
            }
        }
    }
    return hash;
}
audio::AudioBuffer create_source(int hz, int seconds)
{
    auto rate = core::SampleRate::create(hz);
    require(bool(rate), "unsupported rate");
    auto fmt = audio::AudioFormat::create(
        *rate.value(), audio::ChannelLayout::STEREO_LR);
    require(bool(fmt), "format not supported");
    const auto frames = static_cast<std::int64_t>(hz) * seconds;
    auto source = audio::AudioBuffer::create(
        *fmt.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{kOrigin}, frame_count(frames));
    require(bool(source), "cannot allocate synthetic PCM");
    const auto left = source.value()->mutable_view().channel(0);
    const auto right = source.value()->mutable_view().channel(1);
    require(bool(left) && bool(right), "no source channel");
    // Fixed deterministic broadband, transient-rich, high-TP challenge.
    // Generated OUTSIDE all timed P2 candidate evaluations.
    std::uint32_t noise = 0xC001D00DU;
    for (std::int64_t i = 0; i < frames; ++i) {
        noise = 1664525U * noise + 1013904223U;
        const double unitNoise =
            static_cast<double>((noise >> 8) & 0x00FFFFFFU)
            / static_cast<double>(0x01000000U) - 0.5;
        const double t = static_cast<double>(i) / hz;
        const double transient = (i % (hz / 3) < 60) ? 0.47 : 0.0;
        const double l = 0.68 * std::sin(2.0 * kPi * 79.0 * t)
                       + 0.33 * std::sin(2.0 * kPi * 7189.0 * t)
                       + 0.15 * unitNoise + transient;
        (*left.value())[static_cast<std::size_t>(i)] = l;
        (*right.value())[static_cast<std::size_t>(i)] =
            -0.61 * l + 0.28 * std::sin(2.0 * kPi * 1103.0 * t);
    }
    return std::move(*source.value());
}
struct Probe {
    double prepareMs{}, processMs{}, finalizeMs{}, totalMs{};
    std::int64_t latency{};
    std::uint64_t pcmFingerprint{};
};
Probe execute(const audio::AudioBuffer& input, int hz, double drive,
              bool bypass)
{
    // One brand-new epoch / module / output per independent candidate.
    const auto n = input.view().frame_count().value();
    const dsp::DspProcessSpec spec{
        input.view().format(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        frame_count(kBlock)};
    const double ceilingDbTp = -1.0;
    const Configuration cfg{hz, 2, 2,
        std::pow(10.0, ceilingDbTp / 20.0), drive};
    ResearchP2IModule p2{p2_descriptor(), cfg, bypass};
    dsp::IModule& module = p2;
    const auto t0 = Clock::now();
    const auto requirements = module.runtime_requirements(spec);
    require(bool(requirements), "P2 requirements failed");
    const auto delay =
        requirements.value()->algorithmic_latency_frames.value();
    require(delay > 0, "P2 latency invalid");
    require(bool(module.prepare(spec)), "P2 prepare failed");
    const auto t1 = Clock::now();
    auto out = audio::AudioBuffer::create(
        input.view().format(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{kOrigin}, frame_count(n + delay));
    require(bool(out), "P2 output allocation failed");
    for (std::int64_t offset = 0; offset < n;) {
        const auto len = std::min<std::int64_t>(kBlock, n - offset);
        const auto pos = kOrigin + offset;
        const auto iv = input.view().subview(
            core::FrameIndex{pos}, frame_count(len));
        auto ov = out.value()->mutable_view().subview(
            core::FrameIndex{pos}, frame_count(len));
        require(bool(iv) && bool(ov), "P2 subview failed");
        require(bool(module.process(*iv.value(), *ov.value(), {
            frame_range(pos, len), offset == 0, false})),
            "P2 process rejected a block");
        offset += len;
    }
    const auto t2 = Clock::now();
    for (std::int64_t offset = 0; offset < delay;) {
        const auto len = std::min<std::int64_t>(kBlock, delay - offset);
        const auto pos = kOrigin + n + offset;
        auto ov = out.value()->mutable_view().subview(
            core::FrameIndex{pos}, frame_count(len));
        require(bool(ov), "P2 tail subview failed");
        require(bool(module.finalize(*ov.value(), {
            frame_range(pos, len), false, offset + len == delay})),
            "P2 finalize rejected a tail block");
        offset += len;
    }
    const auto t3 = Clock::now();
    Probe probe{};
    probe.prepareMs = elapsed_ms(t0, t1);
    probe.processMs = elapsed_ms(t1, t2);
    probe.finalizeMs = elapsed_ms(t2, t3);
    probe.totalMs = elapsed_ms(t0, t3);
    probe.latency = delay;
    probe.pcmFingerprint = fingerprint(*out.value());
    return probe;
}
int main(int argc, char** argv)
{
    try {
        bool quick = false;
        std::string outputPath = "P2_AUTOMATCH_5MIN_BENCH.tsv";
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            if (arg == "--quick") quick = true;
            else if (arg == "--output" && i + 1 < argc)
                outputPath = argv[++i];
            else throw std::runtime_error("usage: [--quick] [--output PATH]");
        }
        std::ofstream file{outputPath, std::ios::out | std::ios::trunc};
        require(bool(file), "cannot open benchmark TSV");
        file << "schema\trate_hz\tduration_s\tframes\tcandidate"
                "\tdrive_db\tbypass\tprepare_ms\tprocess_ms"
                "\tfinalize_ms\ttotal_ms\tlatency_frames"
                "\toutput_fingerprint\n";
        file << std::fixed << std::setprecision(3);
        const int seconds = quick ? 2 : 300;
        const std::array<double, 4> drives{0.0, 3.0, 6.0, 0.0};
        for (int hz : {44100, 48000}) {
            const auto input = create_source(hz, seconds);
            std::uint64_t originalHash = 0;
            for (int j = 0; j < (quick ? 2 : 5); ++j) {
                const bool bypass = j == 4;
                const double drive = drives[static_cast<std::size_t>(j % 4)];
                const auto probe = execute(input, hz, drive, bypass);
                if (j == 0) originalHash = probe.pcmFingerprint;
                if (j == 3) require(originalHash == probe.pcmFingerprint,
                    "fresh-epoch repeat A fingerprint mismatch");
                file << "I2-PERF-1\t" << hz << '\t' << seconds << '\t'
                     << static_cast<std::int64_t>(hz) * seconds
                     << '\t' << j << '\t' << drive << '\t'
                     << (bypass ? 1 : 0) << '\t' << probe.prepareMs
                     << '\t' << probe.processMs << '\t'
                     << probe.finalizeMs << '\t' << probe.totalMs
                     << '\t' << probe.latency << '\t'
                     << std::hex << probe.pcmFingerprint << std::dec << '\n';
                std::cout << "fs=" << hz << " seconds=" << seconds
                          << " candidate=" << j << " drive=" << drive
                          << " bypass=" << bypass
                          << " P2_process_ms=" << probe.processMs
                          << " P2_total_ms=" << probe.totalMs << '\n';
                file.flush();
                require(bool(file), "cannot write benchmark TSV");
            }
        }
        std::cout << "R1d-P2 isolated full-song candidate benchmark COMPLETE"
                  << " output=" << outputPath
                  << " (synthetic, steady_clock, not AutoMatch WCET)\n";
        return 0;
    } catch (const std::exception& err) {
        std::cerr << "R1d-P2 performance probe FAILED: " << err.what()
                  << '\n';
        return 1;
    }
}
