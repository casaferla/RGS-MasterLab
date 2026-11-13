// RGS MasterLab R1d-P2 research-only: Product RenderPreview offline chain
// benchmark. No canonical L3 solver, no product implementation, no WAV export.
// Original audio, DSP, ModuleRegistry and renderer are executed UNMODIFIED.
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/core/uuid.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <rgsml/dsp/streaming_true_peak_limiter_parameters.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_result.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace rgsml;
using Clock = std::chrono::steady_clock;
constexpr double kPi = 3.14159265358979323846264338327950288;
constexpr std::int64_t kOrigin = 137;
constexpr std::int64_t kBlock = 1024;
constexpr std::array<const char*, 5> kTypes{
    "rgsml.dsp.gain", "rgsml.dsp.parametric-eq",
    "rgsml.dsp.compressor", "rgsml.dsp.stereo-ms",
    "rgsml.dsp.streaming-true-peak-limiter"};

void require(bool ok, const char* reason)
{
    if (!ok) throw std::runtime_error(reason);
}
core::FrameCount fc(std::int64_t count)
{
    auto r = core::FrameCount::create(count);
    require(bool(r), "FrameCount failed");
    return *r.value();
}
core::FrameRange fr(std::int64_t from, std::int64_t to)
{
    auto r = core::FrameRange::create(
        core::FrameIndex{from}, core::FrameIndex{to});
    require(bool(r), "FrameRange failed");
    return *r.value();
}
dsp::ModuleInstanceId id(unsigned slot)
{
    core::Uuid::Bytes b{};
    b[15] = static_cast<std::uint8_t>(slot);
    const auto r = dsp::ModuleInstanceId::from_uuid(core::Uuid{b});
    require(bool(r), "Module ID failed");
    return *r.value();
}
double millis(Clock::time_point a, Clock::time_point b)
{
    return std::chrono::duration<double, std::milli>{b - a}.count();
}
audio::AudioBuffer synthetic(int fs, int seconds)
{
    auto rate = core::SampleRate::create(fs);
    require(bool(rate), "SampleRate failed");
    const auto format = audio::AudioFormat::create(
        *rate.value(), audio::ChannelLayout::STEREO_LR);
    require(bool(format), "AudioFormat failed");
    const auto n = static_cast<std::int64_t>(fs) * seconds;
    auto result = audio::AudioBuffer::create(
        *format.value(), audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{kOrigin}, fc(n));
    require(bool(result), "source allocation failed");
    const auto l = result.value()->mutable_view().channel(0);
    const auto r = result.value()->mutable_view().channel(1);
    require(bool(l) && bool(r), "PCM lanes unavailable");
    std::uint32_t noise = 0xC001D00DU;
    for (std::int64_t i = 0; i < n; ++i) {
        noise = 1664525U * noise + 1013904223U;
        const double white = static_cast<double>((noise >> 8) & 0xFFFFFFU)
                           / static_cast<double>(0x1000000U) - 0.5;
        const double t = static_cast<double>(i) / fs;
        const double attack = (i % (fs / 3) < 60) ? 0.47 : 0.0;
        const double left = 0.68 * std::sin(2 * kPi * 79 * t)
                          + 0.33 * std::sin(2 * kPi * 7189 * t)
                          + 0.15 * white + attack;
        (*l.value())[static_cast<std::size_t>(i)] = left;
        (*r.value())[static_cast<std::size_t>(i)] =
            -0.61 * left + 0.28 * std::sin(2 * kPi * 1103 * t);
    }
    return std::move(*result.value());
}
std::uint64_t fingerprint(audio::AudioBufferView v)
{
    // Not a cryptographic hash. Work is excluded from render timing.
    std::uint64_t result = 14695981039346656037ULL;
    for (unsigned c = 0; c < 2; ++c) {
        auto lane = v.channel(c);
        require(bool(lane), "fingerprint channel missing");
        for (double val : *lane.value()) {
            require(std::isfinite(val), "non-finite PCM");
            const auto bits = std::bit_cast<std::uint64_t>(val);
            for (unsigned shift = 0; shift < 64; shift += 8) {
                result ^= (bits >> shift) & 255U;
                result *= 1099511628211ULL;
            }
        }
    }
    return result;
}
struct Trial {
    const char* name;
    int stages;
    bool p2Bypass;
    double drive;
    double ceilingDbTp{-1.0};
    double gainDb{1.0};
};
constexpr std::array<Trial, 10> kTrials{{
    {"GAIN", 1, true, 0.0},
    {"GAIN_EQ", 2, true, 0.0},
    {"GAIN_EQ_COMP", 3, true, 0.0},
    {"GAIN_EQ_COMP_MS", 4, true, 0.0},
    {"FULL_P2_BYPASS", 5, true, 0.0},
    {"FULL_P2_DRIVE0", 5, false, 0.0},
    {"FULL_P2_DRIVE3", 5, false, 3.0},
    {"FULL_P2_DRIVE0_REPEAT", 5, false, 0.0},
    {"P2_ONLY", -1, false, 0.0},
    {"P2_ONLY_BYPASS", -1, true, 0.0}
}};
struct Outcome {
    double ms{};
    std::uint64_t hash{};
    std::size_t signatures{};
};
Outcome evaluate(const audio::AudioBuffer& source,
                 const dsp::ModuleRegistry& registry, const Trial& trial,
                 std::vector<double>* rawCaptured = nullptr)
{
    auto chain = dsp::ProcessingChain::create(
        registry, {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    require(bool(chain), "ProcessingChain failed");

    const auto gain = dsp::GainParameters::create(trial.gainDb);
    require(bool(gain), "Gain params failed");
    core::Uuid::Bytes uuid{};
    uuid[15] = 77;
    auto eqBand = dsp::EqBandParameters::create(
        core::Uuid{uuid}, true, dsp::EqFilterType::BELL,
        dsp::EqRouting::STEREO, dsp::BellPayload{1000.0, 2.0, 0.9});
    require(bool(eqBand), "EQ band failed");
    std::vector<dsp::EqBandParameters> bands;
    bands.push_back(std::move(*eqBand.value()));
    auto eq = dsp::ParametricEqParameters::create(std::move(bands));
    require(bool(eq), "EQ params failed");
    auto comp = dsp::CompressorParameters::create_default();
    require(bool(comp), "Compressor params failed");
    auto ms = dsp::StereoMsParameters::create(
        0.0, 1.8, false, dsp::MonoBassMode::OFF, 120.0, 100.0);
    require(bool(ms), "Stereo MS params failed");
    auto p2 = dsp::StreamingTruePeakLimiterParameters::create(
        trial.ceilingDbTp, trial.drive, 2);
    require(bool(p2), "P2 params failed");
    std::vector<dsp::ModuleExecutionBinding> bindings;
    const int first = trial.stages == -1 ? 4 : 0;
    const int last = trial.stages == -1 ? 5 : trial.stages;
    for (int stage = first; stage < last; ++stage) {
        auto mid = id(static_cast<unsigned>(stage + 1));
        require(bool(chain.value()->add(
            mid, kTypes[static_cast<std::size_t>(stage)],
            static_cast<std::size_t>(stage - first))),
            "Chain add failed");
        if (stage == 4 && trial.p2Bypass)
            require(bool(chain.value()->set_user_bypass(mid, true)),
                    "P2 bypass failed");
        switch (stage) {
        case 0: bindings.push_back({mid, *gain.value()}); break;
        case 1: bindings.push_back({mid, *eq.value()}); break;
        case 2: bindings.push_back({mid, *comp.value()}); break;
        case 3: bindings.push_back({mid, *ms.value()}); break;
        case 4: bindings.push_back({mid, *p2.value()}); break;
        default: throw std::runtime_error("unrecognized module");
        }
    }
    auto in = source.view();
    auto req = render::RenderRequest::create(
        in, fr(kOrigin, in.absolute_end_frame().value()), *chain.value(),
        std::move(bindings), fc(kBlock));
    require(bool(req), "RenderRequest failed");
    const auto t0 = Clock::now();
    auto result = render::render_preview(*req.value(), registry);
    const auto t1 = Clock::now();
    require(bool(result), "genuine Product RenderPreview failed");
    require(result.value()->signatures().size()
        == static_cast<std::size_t>(last - first),
        "RenderPreview signatures missing");
    // Outside the timing interval, preserve every raw PCM bit for an exact
    // comparison. Never use only fingerprints as a cache acceptance gate.
    if (rawCaptured) {
        const auto v = result.value()->view();
        const auto frames = static_cast<std::size_t>(v.frame_count().value());
        rawCaptured->resize(2 * frames);
        for (int channel = 0; channel < 2; ++channel) {
            const auto lane = v.channel(channel);
            require(bool(lane), "cache proof capture has no channel");
            require(lane.value()->size() == frames, "bad capture length");
            std::copy(lane.value()->begin(), lane.value()->end(),
                rawCaptured->begin() + static_cast<std::size_t>(channel) * frames);
        }
    }
    return Outcome{millis(t0, t1),
        fingerprint(result.value()->view()),
        result.value()->signatures().size()};
}
// Research-only stage-boundary feasibility: PCM bit equality alone.
// Output after four Product DSPs is replayed as P2-only source without
// altering the frozen renderer. Additional metadata/sidecars still require
// validation before any cache can be built in L2/L3.
void cache_boundary_proof(const dsp::ModuleRegistry& registry,
                          int fs, int seconds, std::ofstream& report,
                          Trial suffix=Trial{"CACHED_SUFFIX_P2",-1,false,3.0},
                          bool staleGain=false, bool staleSource=false,
                          bool matrix=false)
{
    auto prepared=synthetic(fs,seconds);
    std::vector<double> upstream,fresh,cached;
    const Trial firstFour{"UNCACHED_UPSTREAM_4",4,true,0.0};
    const Trial fullTrial{"FRESH_FULL_5",5,suffix.p2Bypass,
        suffix.drive,suffix.ceilingDbTp,staleGain?2.0:1.0};
    const auto base=evaluate(prepared,registry,firstFour,&upstream);
    if(staleSource) {
        // Compare full rendering of NEW source with stale OLD stage PCM.
        const auto lane=prepared.mutable_view().channel(0);
        require(bool(lane),"stale source lane missing");
        const auto start=static_cast<std::size_t>(fs/2);
        for(std::size_t i=start;
            i<std::min(lane.value()->size(),start+1024U);++i)
            (*lane.value())[i]+=0.5;
    }
    const auto full=evaluate(prepared,registry,fullTrial,&fresh);
    const auto rate=core::SampleRate::create(fs);
    require(bool(rate),"invalid sample rate");
    const auto fmt=audio::AudioFormat::create(
        *rate.value(),audio::ChannelLayout::STEREO_LR);
    require(bool(fmt),"invalid format");
    const auto n=static_cast<std::size_t>(fs)*
        static_cast<std::size_t>(seconds);
    require(upstream.size()==2*n && fresh.size()==2*n,
        "cache proof source or fresh PCM shape invalid");
    auto cachedStage=audio::AudioBuffer::create(
        *fmt.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{kOrigin},fc(static_cast<std::int64_t>(n)));
    require(bool(cachedStage),"cache stage buffer allocation failed");
    for(std::size_t channel=0;channel<2;++channel) {
        const auto lane=cachedStage.value()->mutable_view().channel(
            static_cast<int>(channel));
        require(bool(lane)&&lane.value()->size()==n,
            "cache stage PCM lane missing");
        std::copy(upstream.begin()+channel*n,
                  upstream.begin()+(channel+1)*n,lane.value()->begin());
    }
    const auto reused=evaluate(*cachedStage.value(),registry,suffix,&cached);
    require(cached.size()==fresh.size(),"cache/full length mismatch");
    std::size_t mismatchCount=0,firstMismatch=0;
    for(std::size_t i=0;i<fresh.size();++i) {
        if(std::bit_cast<std::uint64_t>(fresh[i])!=
            std::bit_cast<std::uint64_t>(cached[i])) {
            if(mismatchCount==0) firstMismatch=i;
            ++mismatchCount;
        }
    }
    const bool exact=mismatchCount==0;
    const bool stale=staleSource||staleGain;
    const char* observed=exact?"EXACT_ACCEPT":"NON_EQUIVALENT_REJECT";
    const char* expected=stale?"NON_EQUIVALENT_REJECT":"EXACT_ACCEPT";
    if(matrix) {
        report<<"I2-CACHE-MATRIX-1\t"<<fs<<'\t'<<seconds<<'\t'
              <<(staleGain?"STALE_GAIN":staleSource?"STALE_SOURCE":
                suffix.p2Bypass?"P2_BYPASS":
                suffix.ceilingDbTp< -1.1?"P2_CEILING_MINUS3":
                suffix.drive==0?"P2_DRIVE0":
                suffix.drive==3?"P2_DRIVE3":"P2_DRIVE6")
              <<'\t'<<suffix.drive<<'\t'<<suffix.ceilingDbTp<<'\t'
              <<(suffix.p2Bypass?1:0)<<'\t'
              <<expected<<'\t'<<observed;
    } else {
        report<<"I2-CACHE-BOUNDARY-1\t"<<fs<<'\t'<<seconds
              <<"\tPOST_TRIM_TO_P2\t"<<observed;
    }
    report<<'\t'<<mismatchCount<<'\t'
          <<(exact?-1:static_cast<std::int64_t>(firstMismatch%n))<<'\t'
          <<(exact?-1:static_cast<std::int64_t>(firstMismatch/n))<<'\t'
          <<base.ms<<'\t'<<full.ms<<'\t'<<reused.ms<<'\t'
          <<std::hex<<base.hash<<'\t'<<full.hash<<'\t'
          <<reused.hash<<std::dec<<'\n';
    report.flush();
    require(bool(report),"cache matrix write failed");
    std::cout<<"CACHE_PROOF "<<fs<<" result "<<observed
             <<" mismatches "<<mismatchCount<<" full_ms "<<full.ms
             <<" suffix_ms "<<reused.ms<<'\n';
    if(matrix)
        require(exact!=stale,
            "cache matrix failed expected exact/stale classification");
}

int main(int argc, char** argv)
{
    try {
        bool quick = false;
        bool cacheProof = false;
        bool cacheMatrix = false;
        int evalCount = 0;
        int evaluationRate = 48000;
        std::string path = "P2_AUTOMATCH_CHAIN_5MIN_BENCH.tsv";
        for (int i = 1; i < argc; ++i) {
            const std::string_view argument{argv[i]};
            if (argument == "--quick") quick = true;
            else if (argument == "--cache-proof") cacheProof = true;
            else if (argument == "--cache-matrix") cacheMatrix = true;
            else if (argument == "--output" && i + 1 < argc)
                path = argv[++i];
            else if (argument == "--eval-count" && i + 1 < argc)
                evalCount = std::stoi(argv[++i]);
            else if (argument == "--rate" && i + 1 < argc)
                evaluationRate = std::stoi(argv[++i]);
            else throw std::runtime_error(
                "Usage: [--quick] [--output PATH] "
                "[--eval-count 1..150] [--rate 44100|48000] "
                "[--cache-proof|--cache-matrix]");
        }
        auto normal = dsp::ModuleRegistry::create_dsp_package_v1();
        require(bool(normal), "Product registry failed");
        auto optin = dsp::ModuleRegistry::with_streaming_true_peak_preview(
            std::move(*normal.value()));
        require(bool(optin), "P2 optin registry failed");
        require(evalCount >= 0 && evalCount <= 150
            && (evaluationRate == 44100 || evaluationRate == 48000)
            && !((cacheProof || cacheMatrix) && evalCount != 0)
            && !(cacheProof && cacheMatrix),
            "Invalid evaluation count, rate or cache proof combination");
        const int seconds = quick ? 2 : 300;
        const int trialLimit = quick ? 7 : static_cast<int>(kTrials.size());
        const std::array<double,4> evaluationDrives{0.0,3.0,6.0,0.0};
        std::ofstream file{path, std::ios::out | std::ios::trunc};
        require(bool(file), "Benchmark TSV could not be opened");
        file << "schema\trate_hz\tduration_s\ttrial\tstages"
                "\tp2_bypass\tp2_drive_db\tfull_render_ms"
                "\tsignatures\tpcm_fingerprint\n";
        file << std::fixed << std::setprecision(3);
        if(cacheProof || cacheMatrix) {
            // Separate TSV schemas for the legacy one-case proof and matrix.
            file.close();
            file.open(path,std::ios::out|std::ios::trunc);
            require(bool(file),"cache report open failed");
            if(cacheMatrix)
                file<<"schema\trate_hz\tduration_s\tcase\tp2_drive_db"
                       "\tceiling_db_tp\tp2_bypass\texpected\tobserved";
            else
                file<<"schema\trate_hz\tduration_s\tboundary\tresult";
            file<<"\tmismatch_samples\tfirst_mismatch_frame"
                   "\tfirst_mismatch_channel\tupstream_ms\tfull_ms"
                   "\tsuffix_ms\tupstream_hash\tfull_hash\tsuffix_hash\n";
            file<<std::fixed<<std::setprecision(3);
            for(int fs:{44100,48000}) {
                if (!cacheMatrix && fs != evaluationRate) continue;
                if(cacheProof) {
                    cache_boundary_proof(*optin.value(),fs,seconds,file);
                } else {
                    constexpr std::array<Trial,5> p2Options{{
                        {"P2_ONLY",-1,false,0.0},
                        {"P2_ONLY",-1,false,3.0},
                        {"P2_ONLY",-1,false,6.0},
                        {"P2_ONLY",-1,false,3.0,-3.0},
                        {"P2_BYPASS",-1,true,0.0}
                    }};
                    for(const auto& t:p2Options)
                        cache_boundary_proof(*optin.value(),fs,seconds,
                                             file,t,false,false,true);
                    cache_boundary_proof(*optin.value(),fs,seconds,file,
                        Trial{"P2_ONLY",-1,false,3.0},true,false,true);
                    cache_boundary_proof(*optin.value(),fs,seconds,file,
                        Trial{"P2_ONLY",-1,false,3.0},false,true,true);
                }
            }
            std::cout<<"CACHE_STAGE_BOUNDARY_PROOF_COMPLETE; "
                     <<"DO NOT PROMOTE ON PCM-ONLY IDENTITY\n";
            return 0;
        }
        for (const int hz : {44100, 48000}) {
            if (evalCount > 0 && hz != evaluationRate) continue;
            const auto input = synthetic(hz, seconds);
            const auto unchanged = fingerprint(input.view());
            std::optional<std::uint64_t> initialP2;
            for (int index = 0;
                 index < (evalCount > 0 ? evalCount : trialLimit); ++index) {
                // Deliberately repeat the same immutable 5-node chain with
                // only P2 Drive changed. This is throughput-only, NOT
                // an L3-M10 coordinate descent or actual cache benchmark.
                const Trial simulation{"SIM_FULL_P2", 5, false,
                    evaluationDrives[static_cast<std::size_t>(index % 4)]};
                const auto& trial = evalCount > 0
                    ? simulation : kTrials[static_cast<std::size_t>(index)];
                const auto measured = evaluate(input, *optin.value(), trial);
                if (evalCount == 0 && index == 5)
                    initialP2 = measured.hash;
                if (evalCount == 0 && index == 7)
                    require(initialP2 && *initialP2 == measured.hash,
                            "P2 repeat candidate differs from A");
                if (evalCount > 0 && index == 0)
                    initialP2 = measured.hash;
                if (evalCount > 0 && index > 0
                    && index % 4 == 0)
                    require(initialP2 && *initialP2 == measured.hash,
                            "Repeated simulation drive0 changed PCM");
                file << "I2-CHAIN-PERF-1\t" << hz << '\t'
                     << seconds << '\t' << trial.name << '\t'
                     << (trial.stages == -1 ? 1 : trial.stages) << '\t'
                     << (trial.p2Bypass ? 1 : 0) << '\t'
                     << trial.drive << '\t' << measured.ms << '\t'
                     << measured.signatures << '\t' << std::hex
                     << measured.hash << std::dec << '\n';
                file.flush();
                require(bool(file), "Benchmark TSV could not be written");
                std::cout << "rate=" << hz << " scenario=" << trial.name
                          << " real_render_ms=" << measured.ms << '\n';
            }
            require(unchanged == fingerprint(input.view()),
                    "RenderPreview modified source PCM");
        }
        std::cout << "RGS P2 I2 REAL PRODUCT CHAIN BENCH PASS: "
                  << path << "; no analysis or L3 solver\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "RGS P2 I2 REAL PRODUCT CHAIN BENCH FAIL: "
                  << e.what() << '\n';
        return 1;
    }
}
