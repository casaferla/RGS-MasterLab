// M22 NONRELEASE: actual canonical MANUAL DSP RenderResult -> frozen original
// Final SRC Q0 -> genuine bound M17 terminal/M16, without duplicate DSP.
#include "render_test_support.hpp"
#include "m22_full_chain_witness_candidate.hpp"
#include "m22_verified_full_output_window_candidate.hpp"
#include "m18_terminal_sample_codec.hpp"
#include <rgsml/audio/phase11_final_src_candidate.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/compressor_parameters.hpp>
#include <rgsml/dsp/parametric_eq_parameters.hpp>
#include <rgsml/dsp/stereo_ms_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef RGSML_M22_SOURCE_DIR
#error "The real approved FIR asset root must be configured"
#endif

namespace rgsml::tests {
class M22RealManualQ0M17ChainCandidateTest final : public QObject {
    Q_OBJECT
private:
    [[nodiscard]] static std::vector<std::uint8_t> frozen(
        std::int64_t from, std::int64_t to) {
        const auto common=std::gcd(from,to);
        const auto L=to/common;
        const auto M=from/common;
        const auto name="RGSMSR1_L"+std::to_string(L)+"_M"+
                        std::to_string(M)+".bin";
        const auto path=std::filesystem::path{RGSML_M22_SOURCE_DIR}
                        /"resources"/"fir"/name;
        std::ifstream stream{path,std::ios::binary};
        if(!stream)return {};
        return {std::istreambuf_iterator<char>{stream},
                std::istreambuf_iterator<char>{}};
    }

    [[nodiscard]] static rgsml::core::Result<rgsml::audio::AudioBuffer>
    source(std::int64_t hz, rgsml::audio::ChannelLayout layout) {
        using namespace rgsml;
        auto rate=core::SampleRate::create(hz);
        if(!rate)return core::Result<audio::AudioBuffer>::failure(*rate.error());
        auto format=audio::AudioFormat::create(*rate.value(),layout);
        if(!format)return core::Result<audio::AudioBuffer>::failure(*format.error());
        auto count=core::FrameCount::create(257);
        if(!count)return core::Result<audio::AudioBuffer>::failure(*count.error());
        auto result=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},*count.value());
        if(!result)return result;
        for(std::size_t ch=0;ch<result.value()->view().format().channel_count();++ch) {
            auto plane=result.value()->mutable_view().channel(ch);
            if(!plane)return core::Result<audio::AudioBuffer>::failure(*plane.error());
            for(std::size_t i=0;i<plane.value()->size();++i) {
                // Deliberately below M17 ceiling; exercise the actual
                // preterminal and final-check code, not fabricated witness.
                (*plane.value())[i]=0.1*std::sin(
                    static_cast<double>((i+ch*11U)%37U)*0.1);
            }
        }
        return result;
    }
private slots:
    void realFourModuleStatefulManualFullPreviewAndCheckedM17Parity() {
        // Actual DSP instances in stored order, not artificial histories:
        // source -> Gain -> EQ -> Stereo/M-S (active mono bass LR12)
        // -> Compressor -> real M17 -> real M16, then true PCM crop.
        using namespace rgsml;
        using namespace render_support;
        constexpr std::int64_t n=4096,fs=48000;
        auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
        QVERIFY(registry);
        auto rate=core::SampleRate::create(fs);
        QVERIFY(rate);
        auto format=audio::AudioFormat::create(
            *rate.value(),audio::ChannelLayout::STEREO_LR);
        QVERIFY(format);
        auto original=audio::AudioBuffer::create(
            *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{0},frame_count(n));
        QVERIFY(original);
        for(std::size_t ch=0;ch<2;++ch) {
            auto plane=original.value()->mutable_view().channel(ch);
            QVERIFY(plane);
            for(std::size_t i=0;i<plane.value()->size();++i) {
                const auto t=static_cast<double>(i);
                (*plane.value())[i]=
                    0.12*std::sin(t*(ch==0?0.030:0.037))
                   +0.06*std::cos(t*(ch==0?0.009:0.012))
                   +((i%499U)==0U?0.24:0.0);
            }
        }
        const auto immutableSource=bits(original.value()->view());
        auto manual=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        auto terminal=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
        QVERIFY(manual&&terminal);
        const auto gainId=make_id("95000000-0000-4000-8000-000000000001");
        const auto eqId=make_id("95000000-0000-4000-8000-000000000002");
        const auto msId=make_id("95000000-0000-4000-8000-000000000003");
        const auto compId=make_id("95000000-0000-4000-8000-000000000004");
        const auto limiterId=make_id("95000000-0000-4000-8000-000000000017");
        QVERIFY(manual.value()->add(gainId,"rgsml.dsp.gain",0));
        QVERIFY(manual.value()->add(eqId,"rgsml.dsp.parametric-eq",1));
        QVERIFY(manual.value()->add(msId,"rgsml.dsp.stereo-ms",2));
        QVERIFY(manual.value()->add(compId,"rgsml.dsp.compressor",3));
        QVERIFY(terminal.value()->add(
            limiterId,"rgsml.dsp.true-peak-limiter",0));
        const auto gain=dsp::GainParameters::create(-5.0);
        const auto bandId=*core::Uuid::parse(
            "95000000-0000-4000-8000-000000000042").value();
        const auto band=dsp::EqBandParameters::create(
            bandId,true,dsp::EqFilterType::BELL,dsp::EqRouting::STEREO,
            dsp::BellPayload{700.0,3.0,0.9});
        QVERIFY(gain&&band);
        const auto eq=dsp::ParametricEqParameters::create({*band.value()});
        const auto ms=dsp::StereoMsParameters::create(
            -3.0,6.0,false,dsp::MonoBassMode::LR12,145.0,35.0);
        const auto compressor=dsp::CompressorParameters::create_default();
        const auto ceiling=dsp::TruePeakLimiterParameters::create(-1.0);
        QVERIFY(eq&&ms&&compressor&&ceiling);
        const std::vector<dsp::ModuleExecutionBinding> binding{
            {compId,*compressor.value()},
            {msId,*ms.value()},
            {gainId,*gain.value()},
            {eqId,*eq.value()}};
        const std::array<dsp::ModuleExecutionBinding,1> terminalBinding{{
            {limiterId,*ceiling.value()}}};
        std::vector<std::uint64_t> canonicalFull;
        for(const auto chunk:{64,257}) {
            const auto request=render::RenderRequest::create(
                original.value()->view(),
                original.value()->view().absolute_range(),
                *manual.value(),binding,frame_count(chunk));
            QVERIFY(request);
            auto full=render::render_preview(
                *request.value(),*registry.value());
            QVERIFY2(full,full?"":full.error()->message().c_str());
            QCOMPARE(full.value()->signatures().size(),std::size_t{4});
            for(const auto& signature:full.value()->signatures())
                QCOMPARE(signature.disposition,
                         render::ModuleExecutionDisposition::PROCESSED);
            QCOMPARE(full.value()->signatures()[0].instance_id,gainId);
            QCOMPARE(full.value()->signatures()[1].instance_id,eqId);
            QCOMPARE(full.value()->signatures()[2].instance_id,msId);
            QCOMPARE(full.value()->signatures()[3].instance_id,compId);
            QVERIFY(full.value()->compressor_telemetry_sidecar().has_value());
            QCOMPARE(full.value()->stereo_ms_stage_output_sidecars().size(),
                     std::size_t{1});
            auto fullBits=bits(full.value()->view());
            QVERIFY(fullBits!=immutableSource);
            if(!canonicalFull.empty())QCOMPARE(fullBits,canonicalFull);
            else canonicalFull=std::move(fullBits);
            // Canonical state-origin replay for a middle region and
            // a tail that includes true EOS. Previous IIR, mono-bass,
            // envelope and compressor histories must NEVER reset here.
            for(const auto [first,last]:
                {std::pair{713,2751},std::pair{3599,4096}}) {
                const auto region=core::FrameRange::create(
                    core::FrameIndex{first},core::FrameIndex{last});
                QVERIFY(region);
                const auto cropped=full.value()->view().subview(
                    core::FrameIndex{first},frame_count(last-first));
                QVERIFY(cropped);
                const auto previewRequest=render::RenderRequest::create(
                    original.value()->view(),*region.value(),
                    *manual.value(),binding,frame_count(chunk==64?17:113));
                QVERIFY(previewRequest);
                const auto preview=render::render_preview(
                    *previewRequest.value(),*registry.value());
                QVERIFY2(preview,preview?"":preview.error()->message().c_str());
                QCOMPARE(bits(preview.value()->view()),bits(*cropped.value()));
            }
            // The retained WHOLE Manual PCM alone can be a trusted M17
            // predecessor. A mid-region result may never impersonate it.
            auto owner=std::make_shared<const render::RenderResult>(
                std::move(*full.value()));
            const auto checked=render::render_true_peak_limiter_owned_output_rate_candidate(
                owner,*manual.value(),frame_count(n),
                owner->chain_revision(),*rate.value(),*terminal.value(),
                terminalBinding,*registry.value(),frame_count(128));
            QVERIFY2(checked,checked?"":checked.error()->message().c_str());
            m22_private::FullChainWitnessFailure reason{};
            const auto genuine=m22_private::observe_real_complete_chain_candidate(
                *checked.value(),reason);
            QVERIFY(genuine);
            QCOMPARE(reason,m22_private::FullChainWitnessFailure::NONE);
            QCOMPARE(genuine->complete_source_frames,n);
            QCOMPARE(genuine->actual_master_frames,n);
            QCOMPARE(genuine->manual_source_hz,fs);
            QCOMPARE(genuine->final_master_hz,fs);
            QVERIFY(!genuine->used_original_q0);
            QVERIFY(checked.value()->terminal.checked.final_check.has_value());
            QVERIFY(!checked.value()->terminal.checked.terminal.bypassed);
            const auto audible=checked.value()->terminal.checked.terminal
                .compensated_pcm.view();
            QCOMPARE(audible.frame_count().value(),n);
            for(const auto [first,last]:
                {std::pair{713,2751},std::pair{3599,4096}}) {
                const auto region=core::FrameRange::create(
                    core::FrameIndex{first},core::FrameIndex{last});
                QVERIFY(region);
                const auto result=
                    m22_private::observe_verified_full_output_window_candidate(
                        *checked.value(),*region.value());
                QVERIFY(result);
                QCOMPARE(result.value()->exact_output_crop.frame_count().value(),
                         static_cast<std::int64_t>(last-first));
                for(std::size_t ch=0;ch<2;++ch) {
                    const auto whole=audible.channel(ch);
                    const auto part=result.value()->exact_output_crop.channel(ch);
                    QVERIFY(whole&&part);
                    QCOMPARE(part.value()->data(),whole.value()->data()+first);
                }
            }
        }
        QCOMPARE(bits(original.value()->view()),immutableSource);
    }

    void allThirtyRealQ0RoutesViaImmutableManualOwnerAndRealM17Bypass() {
        using namespace rgsml;
        using namespace render_support;
        auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
        QVERIFY(registry);
        auto manual=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        auto terminal=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
        QVERIFY(manual&&terminal);
        const auto gainId=make_id("92000000-0000-4000-8000-000000000001");
        const auto limiterId=make_id("92000000-0000-4000-8000-000000000017");
        QVERIFY(manual.value()->add(gainId,"rgsml.dsp.gain",0));
        QVERIFY(terminal.value()->add(limiterId,"rgsml.dsp.true-peak-limiter",0));
        QVERIFY(terminal.value()->set_user_bypass(limiterId,true));
        auto gain=dsp::GainParameters::create(-6.0);
        auto ceiling=dsp::TruePeakLimiterParameters::create(-1.0);
        QVERIFY(gain&&ceiling);
        const std::array<dsp::ModuleExecutionBinding,1> terminalBinding{{
            {limiterId,*ceiling.value()}}};
        const auto block=frame_count(128);
        constexpr std::array<std::int64_t,6> rates{
            44100,48000,88200,96000,176400,192000};
        std::size_t verified=0;
        for(const auto from:rates) {
            auto input=source(from,audio::ChannelLayout::MONO_C);
            QVERIFY(input);
            auto request=render::RenderRequest::create(
                input.value()->view(),input.value()->view().absolute_range(),
                *manual.value(),{{gainId,*gain.value()}},block);
            QVERIFY(request);
            auto prepared=render::render_preview(*request.value(),*registry.value());
            QVERIFY2(prepared,prepared?"":prepared.error()->message().c_str());
            auto owner=std::make_shared<const render::RenderResult>(
                std::move(*prepared.value()));
            // The real Gain module must have changed the original source.
            QVERIFY(bits(owner->view())!=bits(input.value()->view()));
            const auto immutableSourceBits=bits(owner->view());
            for(const auto to:rates) {
                if(from==to)continue;
                const auto truePin=audio::internal::phase11_frozen_original_fir_pin(
                    static_cast<std::uint32_t>(to/std::gcd(from,to)),
                    static_cast<std::uint32_t>(from/std::gcd(from,to)));
                QVERIFY(truePin);
                const auto bytes=frozen(from,to);
                QVERIFY2(bytes.size()>1000,"Real Phase-11 original Q0 FIR missing");
                const auto target=core::SampleRate::create(to);
                QVERIFY(target);
                auto result=render::render_true_peak_limiter_owned_output_rate_candidate(
                    owner,*manual.value(),frame_count(257),
                    owner->chain_revision(),*target.value(),*terminal.value(),
                    terminalBinding,*registry.value(),block,{},nullptr,false,
                    nullptr,bytes);
                QVERIFY2(result,result?"":result.error()->message().c_str());
                QCOMPARE(result.value()->manual_owner.get(),owner.get());
                QVERIFY(result.value()->used_candidate_src);
                QCOMPARE(result.value()->src_kernel_sha256,
                         std::string_view{truePin->sha256});
                QVERIFY(result.value()->terminal.checked.terminal.bypassed);
                m22_private::FullChainWitnessFailure closed{};
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),closed));
                QCOMPARE(closed,m22_private::FullChainWitnessFailure::INVALID_M17_OR_M16);
                QVERIFY(!result.value()->terminal.checked.final_check.has_value());
                QVERIFY(result.value()->terminal.checked.terminal
                    .applied_gain_frames.empty());
                // Authentic Q0 conversion with M17 bypass is never a
                // verified Product Preview, even with perfectly finite PCM.
                auto blockedWindow=core::FrameRange::create(
                    core::FrameIndex{2},core::FrameIndex{12});
                QVERIFY(blockedWindow);
                QVERIFY(!m22_private::observe_verified_full_output_window_candidate(
                    *result.value(),*blockedWindow.value()));
                const auto out=result.value()->terminal.checked.terminal
                    .compensated_pcm.view();
                QCOMPARE(out.timebase().frame_domain_id(),
                         audio::FrameDomainId::OUTPUT_RATE);
                QCOMPARE(out.absolute_start_frame().value(),std::int64_t{0});
                QCOMPARE(out.format().sample_rate().value(),to);
                QCOMPARE(out.frame_count().value(),
                         (std::int64_t{257}*to+from-1)/from);
                // Independent direct call to the exact SAME frozen Q0
                // implementation: M17 bypass is bitwise identity.
                auto direct=audio::materialize_phase11_scoped_q0_pinned_offline_candidate(
                    owner->view(),*target.value(),block,bytes);
                QVERIFY2(direct,direct?"":direct.error()->message().c_str());
                QCOMPARE(bits(out),bits(direct.value()->pcm.view()));
                QCOMPARE(bits(owner->view()),immutableSourceBits);
                ++verified;
            }
        }
        QCOMPARE(verified,std::size_t{30});
    }

    void allThirtyActiveOriginalQ0RoutesUseGenuineFullM17AndM16() {
        using namespace rgsml;
        using namespace render_support;
        auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
        QVERIFY(registry);
        auto manual=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        auto terminal=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
        QVERIFY(manual&&terminal);
        const auto gainId=make_id("94000000-0000-4000-8000-000000000001");
        const auto limiterId=make_id("94000000-0000-4000-8000-000000000017");
        QVERIFY(manual.value()->add(gainId,"rgsml.dsp.gain",0));
        QVERIFY(terminal.value()->add(limiterId,"rgsml.dsp.true-peak-limiter",0));
        const auto gain=dsp::GainParameters::create(-9.0);
        const auto ceiling=dsp::TruePeakLimiterParameters::create(-1.0);
        QVERIFY(gain&&ceiling);
        const std::array<dsp::ModuleExecutionBinding,1> binding{{
            {limiterId,*ceiling.value()}}};
        const auto block=frame_count(128);
        constexpr std::array<std::int64_t,6> rates{
            44100,48000,88200,96000,176400,192000};
        std::size_t completed=0;
        for(const auto from:rates) {
            auto input=source(from,audio::ChannelLayout::MONO_C);
            QVERIFY(input);
            auto request=render::RenderRequest::create(
                input.value()->view(),input.value()->view().absolute_range(),
                *manual.value(),{{gainId,*gain.value()}},block);
            QVERIFY(request);
            auto prepared=render::render_preview(*request.value(),*registry.value());
            QVERIFY(prepared);
            auto owner=std::make_shared<const render::RenderResult>(
                std::move(*prepared.value()));
            for(const auto to:rates) {
                if(from==to)continue;
                auto target=core::SampleRate::create(to);
                QVERIFY(target);
                const auto bytes=frozen(from,to);
                QVERIFY(bytes.size()>1000);
                auto checked=render::render_true_peak_limiter_owned_output_rate_candidate(
                    owner,*manual.value(),frame_count(257),
                    owner->chain_revision(),*target.value(),*terminal.value(),
                    binding,*registry.value(),block,{},nullptr,false,nullptr,bytes);
                QVERIFY2(checked,checked?"":checked.error()->message().c_str());
                QVERIFY(!checked.value()->terminal.checked.terminal.bypassed);
                QVERIFY(checked.value()->terminal.checked.final_check.has_value());
                m22_private::FullChainWitnessFailure why{};
                const auto actual=m22_private::observe_real_complete_chain_candidate(
                    *checked.value(),why);
                QVERIFY(actual);
                QCOMPARE(why,m22_private::FullChainWitnessFailure::NONE);
                QVERIFY(actual->used_original_q0);
                QCOMPARE(actual->manual_source_hz,from);
                QCOMPARE(actual->final_master_hz,to);
                QCOMPARE(actual->complete_source_frames,std::int64_t{257});
                QCOMPARE(actual->actual_master_frames,
                    (std::int64_t{257}*to+from-1)/from);
                const auto range=core::FrameRange::create(
                    core::FrameIndex{3},core::FrameIndex{26});
                QVERIFY(range);
                const auto crop=m22_private::observe_verified_full_output_window_candidate(
                    *checked.value(),*range.value());
                QVERIFY(crop);
                QCOMPARE(crop.value()->exact_output_crop.frame_count().value(),
                         std::int64_t{23});
                QCOMPARE(crop.value()->exact_output_crop.format().sample_rate().value(),
                         to);
                QVERIFY(!crop.value()->authorizes_release());
                QCOMPARE(crop.value()->full_master_witness.frozen_original_q0_sha256,
                    std::string{checked.value()->src_kernel_sha256});
                ++completed;
            }
        }
        QCOMPARE(completed,std::size_t{30});
    }

    void realActiveM17AfterQ0RunsM16AndRetainsManualOwner() {
        using namespace rgsml;
        using namespace render_support;
        auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
        QVERIFY(registry);
        auto manual=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        auto terminal=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
        QVERIFY(manual&&terminal);
        const auto gainId=make_id("93000000-0000-4000-8000-000000000001");
        const auto limiterId=make_id("93000000-0000-4000-8000-000000000017");
        QVERIFY(manual.value()->add(gainId,"rgsml.dsp.gain",0));
        QVERIFY(terminal.value()->add(limiterId,"rgsml.dsp.true-peak-limiter",0));
        const auto gain=dsp::GainParameters::create(-6.0);
        const auto ceiling=dsp::TruePeakLimiterParameters::create(-1.0);
        QVERIFY(gain&&ceiling);
        const std::array<dsp::ModuleExecutionBinding,1> binding{{
            {limiterId,*ceiling.value()}}};
        const auto block=frame_count(128);
        for(const auto pair:std::array<std::array<std::int64_t,2>,2>{{
                {{44100,48000}},{{48000,44100}}}}) {
            auto input=source(pair[0],audio::ChannelLayout::STEREO_LR);
            QVERIFY(input);
            auto request=render::RenderRequest::create(
                input.value()->view(),input.value()->view().absolute_range(),
                *manual.value(),{{gainId,*gain.value()}},block);
            QVERIFY(request);
            auto prepared=render::render_preview(*request.value(),*registry.value());
            QVERIFY(prepared);
            auto owner=std::make_shared<const render::RenderResult>(
                std::move(*prepared.value()));
            const auto before=bits(owner->view());
            auto target=core::SampleRate::create(pair[1]);
            QVERIFY(target);
            const auto bytes=frozen(pair[0],pair[1]);
            QVERIFY(bytes.size()>1000);
            auto result=render::render_true_peak_limiter_owned_output_rate_candidate(
                owner,*manual.value(),frame_count(257),owner->chain_revision(),
                *target.value(),*terminal.value(),binding,*registry.value(),
                block,{},nullptr,false,nullptr,bytes);
            QVERIFY2(result,result?"":result.error()->message().c_str());
            QVERIFY(!result.value()->terminal.checked.terminal.bypassed);
            m22_private::FullChainWitnessFailure why{};
            const auto witness=m22_private::observe_real_complete_chain_candidate(
                *result.value(),why);
            QVERIFY(witness);
            QCOMPARE(why,m22_private::FullChainWitnessFailure::NONE);
            QVERIFY(!witness->authorizes_release());
            QVERIFY(witness->used_original_q0);
            QCOMPARE(witness->complete_source_frames,std::int64_t{257});
            QCOMPARE(witness->final_master_hz,pair[1]);
            QCOMPARE(witness->manual_source_hz,pair[0]);
            QCOMPARE(witness->actual_master_frames,
                     result.value()->terminal.checked.terminal.compensated_pcm.view()
                         .frame_count().value());
            QCOMPARE(witness->frozen_original_q0_sha256,
                     std::string{result.value()->src_kernel_sha256});
            QVERIFY(result.value()->terminal.checked.final_check.has_value());
            QCOMPARE(result.value()->manual_owner.get(),owner.get());
            const auto out=result.value()->terminal.checked.terminal.compensated_pcm.view();
            QCOMPARE(out.timebase().frame_domain_id(),
                     audio::FrameDomainId::OUTPUT_RATE);
            QCOMPARE(out.format().sample_rate().value(),pair[1]);
            QCOMPARE(result.value()->terminal.checked.terminal.applied_gain_frames.size(),
                     static_cast<std::size_t>(out.frame_count().value()));
            // The SOURCE is 257 frames, but actual 44.1/48 OUTPUT_RATE
            // lengths differ: Preview must crop the VERIFIED final M17
            // programme and preserve binary64 bits (never rerun DSP).
            QVERIFY(out.frame_count().value()!=257);
            auto window=core::FrameRange::create(
                core::FrameIndex{9},core::FrameIndex{41});
            QVERIFY(window);
            const auto crop=m22_private::observe_verified_full_output_window_candidate(
                *result.value(),*window.value());
            QVERIFY(crop);
            QVERIFY(!crop.value()->authorizes_release());
            QVERIFY(crop.value()->full_master_witness.used_original_q0);
            QCOMPARE(crop.value()->full_master_witness.actual_master_frames,
                     out.frame_count().value());
            QCOMPARE(crop.value()->exact_output_crop.frame_count().value(),
                     std::int64_t{32});
            QCOMPARE(crop.value()->exact_output_crop.absolute_start_frame().value(),
                     std::int64_t{9});
            for(std::size_t ch=0;ch<out.format().channel_count();++ch) {
                const auto fullPlane=out.channel(ch);
                const auto previewPlane=crop.value()->exact_output_crop.channel(ch);
                QVERIFY(fullPlane&&previewPlane);
                QCOMPARE(previewPlane.value()->data(),fullPlane.value()->data()+9);
                for(std::size_t i=0;i<previewPlane.value()->size();++i)
                    QCOMPARE(std::bit_cast<std::uint64_t>((*previewPlane.value())[i]),
                             std::bit_cast<std::uint64_t>((*fullPlane.value())[i+9]));
            }
            // M18 real terminal PCM24/IEEE-F32 representation must have
            // exactly the FULL Master crop's bytes. PCM24 Philox is
            // indexed by ABSOLUTE OUTPUT frame, not preview-local zero.
            const auto interleave=[](audio::AudioBufferView view) {
                const auto frames=static_cast<std::size_t>(
                    view.frame_count().value());
                const auto channels=view.format().channel_count();
                std::vector<double> values(frames*channels);
                for(std::size_t ch=0;ch<channels;++ch) {
                    const auto plane=view.channel(ch);
                    if(!plane)return std::vector<double>{};
                    for(std::size_t i=0;i<frames;++i)
                        values[i*channels+ch]=(*plane.value())[i];
                }
                return values;
            };
            const auto fullSamples=interleave(out);
            const auto cropSamples=interleave(crop.value()->exact_output_crop);
            const auto channels=static_cast<std::uint32_t>(
                out.format().channel_count());
            QVERIFY(fullSamples.size()==static_cast<std::size_t>(
                out.frame_count().value())*channels);
            QCOMPARE(cropSamples.size(),std::size_t{32}*channels);
            constexpr std::uint64_t seed=UINT64_C(0x0123456789ABCDEF);
            std::vector<std::uint8_t> fullPcm24(fullSamples.size()*3U);
            std::vector<std::uint8_t> cropPcm24(cropSamples.size()*3U);
            std::vector<std::uint8_t> wrongLocalDither(cropPcm24.size());
            QVERIFY(audio::internal::m18_pcm24_block(
                std::span<const double>{fullSamples},channels,0,seed,
                std::span<std::uint8_t>{fullPcm24})
                ==audio::internal::M18CodecStatus::OK);
            QVERIFY(audio::internal::m18_pcm24_block(
                std::span<const double>{cropSamples},channels,9,seed,
                std::span<std::uint8_t>{cropPcm24})
                ==audio::internal::M18CodecStatus::OK);
            QVERIFY(std::equal(cropPcm24.begin(),cropPcm24.end(),
                fullPcm24.begin()+static_cast<std::ptrdiff_t>(9U*channels*3U)));
            QVERIFY(audio::internal::m18_pcm24_block(
                std::span<const double>{cropSamples},channels,0,seed,
                std::span<std::uint8_t>{wrongLocalDither})
                ==audio::internal::M18CodecStatus::OK);
            QVERIFY(wrongLocalDither!=cropPcm24);
            // Real checked full Master may be chunked arbitrarily in the
            // M18 encoder; 17-frame splits must equal whole-file PCM24.
            std::vector<std::uint8_t> chunkedPcm24(fullPcm24.size());
            for(std::size_t frame=0;
                frame<static_cast<std::size_t>(out.frame_count().value());) {
                const auto remaining=static_cast<std::size_t>(
                    out.frame_count().value())-frame;
                const auto n=std::min(std::size_t{17},remaining);
                QVERIFY(audio::internal::m18_pcm24_block(
                    std::span<const double>{fullSamples}.subspan(
                        frame*channels,n*channels),
                    channels,frame,seed,
                    std::span<std::uint8_t>{chunkedPcm24}.subspan(
                        frame*channels*3U,n*channels*3U))
                    ==audio::internal::M18CodecStatus::OK);
                frame+=n;
            }
            QVERIFY(chunkedPcm24==fullPcm24);
            std::vector<std::uint8_t> fullF32(fullSamples.size()*4U);
            std::vector<std::uint8_t> cropF32(cropSamples.size()*4U);
            QVERIFY(audio::internal::m18_float32_block(
                std::span<const double>{fullSamples},channels,0,
                std::span<std::uint8_t>{fullF32})
                ==audio::internal::M18CodecStatus::OK);
            QVERIFY(audio::internal::m18_float32_block(
                std::span<const double>{cropSamples},channels,9,
                std::span<std::uint8_t>{cropF32})
                ==audio::internal::M18CodecStatus::OK);
            QVERIFY(std::equal(cropF32.begin(),cropF32.end(),
                fullF32.begin()+static_cast<std::ptrdiff_t>(9U*channels*4U)));
            const auto beyond=core::FrameRange::create(
                core::FrameIndex{out.frame_count().value()-1},
                core::FrameIndex{out.frame_count().value()+1});
            QVERIFY(beyond);
            QVERIFY(!m22_private::observe_verified_full_output_window_candidate(
                *result.value(),*beyond.value()));
            // No SOURCE-rate window may be mislabeled as output-rate.
            const auto negative=core::FrameRange::create(
                core::FrameIndex{-1},core::FrameIndex{10});
            QVERIFY(negative);
            QVERIFY(!m22_private::observe_verified_full_output_window_candidate(
                *result.value(),*negative.value()));
            for(const auto& gain_frame:result.value()->terminal.checked.terminal.applied_gain_frames)
                QVERIFY(std::isfinite(gain_frame.gain));
            // A detached manual PCM pointer cannot be replaced by an epoch label.
            {
                const auto retained=result.value()->manual_owner;
                result.value()->manual_owner.reset();
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::NO_MANUAL_OWNER);
                result.value()->manual_owner=retained;
            }
            // Same samples with a different manual-chain revision are stale.
            {
                const auto original=result.value()->manual_chain_revision;
                ++result.value()->manual_chain_revision;
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::PARTIAL_OR_STALE_MANUAL);
                result.value()->manual_chain_revision=original;
            }
            // A forged source frame count cannot legitimize a partial Master.
            {
                const auto original=result.value()->programme_frames;
                result.value()->programme_frames=frame_count(256);
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::PARTIAL_OR_STALE_MANUAL);
                result.value()->programme_frames=original;
            }
            // Authentic PCM but stale master-output clock is never admitted.
            {
                const auto original=result.value()->output_rate;
                result.value()->output_rate=
                    *core::SampleRate::create(pair[0]).value();
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INVALID_OUTPUT_DOMAIN_OR_CLOCK);
                result.value()->output_rate=original;
            }
            // A self-declared SHA, even with correct PCM, is not an asset pin.
            {
                const auto original=result.value()->src_kernel_sha256;
                result.value()->src_kernel_sha256="untrusted-q0-sha";
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
                result.value()->src_kernel_sha256=original;
            }
            // The Product must never promote an unmeasured terminal candidate.
            {
                const auto original=result.value()->terminal.checked.final_check;
                result.value()->terminal.checked.final_check.reset();
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INVALID_M17_OR_M16);
                result.value()->terminal.checked.final_check=original;
            }
            // An asserted bypass can never borrow an active verified M16.
            {
                result.value()->terminal.checked.terminal.bypassed=true;
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INVALID_M17_OR_M16);
                result.value()->terminal.checked.terminal.bypassed=false;
            }
            // Source/output sample alignment requires every gain observation.
            {
                auto& gains=result.value()->terminal.checked.terminal.applied_gain_frames;
                QVERIFY(!gains.empty());
                const auto last=gains.back();
                gains.pop_back();
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INVALID_M17_OR_M16);
                gains.push_back(last);
            }
            // No phantom terminal identity from a zero or unknown revision.
            {
                const auto original=result.value()->terminal.terminal_chain_revision;
                result.value()->terminal.terminal_chain_revision=0;
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INVALID_M17_OR_M16);
                result.value()->terminal.terminal_chain_revision=original;
            }
            // An implicit rate conversion with no named frozen Q0 cannot pass.
            {
                result.value()->used_candidate_src=false;
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
                result.value()->used_candidate_src=true;
            }
            // Every rejected probe must leave the real immutable master intact.
            {
                const auto restored=m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why);
                QVERIFY(restored);
                QCOMPARE(why,m22_private::FullChainWitnessFailure::NONE);
                QCOMPARE(restored->frozen_original_q0_sha256,
                         witness->frozen_original_q0_sha256);
                QCOMPARE(restored->actual_master_frames,witness->actual_master_frames);
                QVERIFY(!restored->authorizes_release());
            }
            QCOMPARE(bits(owner->view()),before);
        }
    }

    void allSixNativeIdentityRatesProduceM16CheckedWithoutFIR() {
        using namespace rgsml;
        using namespace render_support;
        auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
        QVERIFY(registry);
        auto manual=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        auto terminal=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
        QVERIFY(manual&&terminal);
        const auto limiterId=make_id("95000000-0000-4000-8000-000000000017");
        QVERIFY(terminal.value()->add(limiterId,"rgsml.dsp.true-peak-limiter",0));
        auto ceiling=dsp::TruePeakLimiterParameters::create(-1.0);
        QVERIFY(ceiling);
        const std::array<dsp::ModuleExecutionBinding,1> binding{{
            {limiterId,*ceiling.value()}}};
        constexpr std::array<std::int64_t,6> rates{
            44100,48000,88200,96000,176400,192000};
        std::size_t inspected=0;
        for(const auto rate:rates) {
            auto input=source(rate,audio::ChannelLayout::MONO_C);
            QVERIFY(input);
            const auto block=frame_count(128);
            auto request=render::RenderRequest::create(
                input.value()->view(),input.value()->view().absolute_range(),
                *manual.value(),{},block);
            QVERIFY(request);
            auto prepared=render::render_preview(*request.value(),*registry.value());
            QVERIFY(prepared);
            auto owner=std::make_shared<const render::RenderResult>(
                std::move(*prepared.value()));
            const auto bitsBefore=bits(owner->view());
            auto hz=core::SampleRate::create(rate);
            QVERIFY(hz);
            auto result=render::render_true_peak_limiter_owned_same_rate_candidate(
                owner,*manual.value(),frame_count(257),owner->chain_revision(),
                *hz.value(),*terminal.value(),binding,*registry.value(),block);
            QVERIFY2(result,result?"":result.error()->message().c_str());
            m22_private::FullChainWitnessFailure why{};
            const auto witness=m22_private::observe_real_complete_chain_candidate(
                *result.value(),why);
            QVERIFY(witness);
            QCOMPARE(why,m22_private::FullChainWitnessFailure::NONE);
            QVERIFY(!witness->authorizes_release());
            QVERIFY(!witness->used_original_q0);
            QVERIFY(witness->frozen_original_q0_sha256.empty());
            QCOMPARE(witness->manual_source_hz,rate);
            QCOMPARE(witness->final_master_hz,rate);
            QCOMPARE(witness->actual_master_frames,std::int64_t{257});
            QVERIFY(result.value()->terminal.checked.final_check.has_value());
            // All 6 same-rate identities also crop the original checked
            // Master with NO FIR and no second M17 processing.
            auto inner=core::FrameRange::create(
                core::FrameIndex{13},core::FrameIndex{29});
            QVERIFY(inner);
            const auto checkedPreview=
                m22_private::observe_verified_full_output_window_candidate(
                    *result.value(),*inner.value());
            QVERIFY(checkedPreview);
            QVERIFY(!checkedPreview.value()->full_master_witness.used_original_q0);
            QCOMPARE(checkedPreview.value()->exact_output_crop.frame_count().value(),
                     std::int64_t{16});
            const auto originalMaster=
                result.value()->terminal.checked.terminal.compensated_pcm.view();
            for(std::size_t ch=0;ch<originalMaster.format().channel_count();++ch) {
                const auto whole=originalMaster.channel(ch);
                const auto part=checkedPreview.value()->exact_output_crop.channel(ch);
                QVERIFY(whole&&part);
                QCOMPARE(part.value()->data(),whole.value()->data()+13);
            }
            QCOMPARE(bits(owner->view()),bitsBefore);
            // An identity clock must NOT consume or claim any FIR coefficient.
            {
                result.value()->src_kernel_sha256="forged-identity-coefficients";

                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
                result.value()->src_kernel_sha256={};
            }
            // Same-clock output must be proven as exact no-SRC, never reclassified.
            {
                result.value()->used_candidate_src=true;

                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
                result.value()->used_candidate_src=false;
            }
            // Unqualified-but-supported M16 is kept as such; UNSUPPORTED is fatal.
            {
                const auto old=result.value()->terminal.checked.final_check->qualification_status;
                result.value()->terminal.checked.final_check->qualification_status=
                    analysis::TruePeakQualificationStatus::UNSUPPORTED;

                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INVALID_M17_OR_M16);
                result.value()->terminal.checked.final_check->qualification_status=old;
            }
            // After all fail-closed probes, return the identical native witness.
            {

                const auto rechecked=m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why);
                QVERIFY(rechecked);
                QCOMPARE(why,m22_private::FullChainWitnessFailure::NONE);
                QVERIFY(!rechecked->authorizes_release());
                QVERIFY(!rechecked->used_original_q0);
                QCOMPARE(rechecked->actual_master_frames,std::int64_t{257});
            }
            ++inspected;
        }
        QCOMPARE(inspected,std::size_t{6});
    }

    void tamperAndStaleManualEpochCannotPublishM17() {
        using namespace rgsml;
        using namespace render_support;
        auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
        QVERIFY(registry);
        auto manual=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        auto terminal=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::TERMINAL});
        QVERIFY(manual&&terminal);
        const auto limiterId=make_id("94000000-0000-4000-8000-000000000017");
        QVERIFY(terminal.value()->add(limiterId,"rgsml.dsp.true-peak-limiter",0));
        QVERIFY(terminal.value()->set_user_bypass(limiterId,true));
        auto ceiling=dsp::TruePeakLimiterParameters::create(-1.0);
        QVERIFY(ceiling);
        const std::array<dsp::ModuleExecutionBinding,1> binding{{
            {limiterId,*ceiling.value()}}};
        auto input=source(44100,audio::ChannelLayout::MONO_C);
        QVERIFY(input);
        const auto block=frame_count(128);
        auto request=render::RenderRequest::create(
            input.value()->view(),input.value()->view().absolute_range(),
            *manual.value(),{},block);
        QVERIFY(request);
        auto prepared=render::render_preview(*request.value(),*registry.value());
        QVERIFY(prepared);
        auto owner=std::make_shared<const render::RenderResult>(
            std::move(*prepared.value()));
        const auto before=bits(owner->view());
        auto rate=core::SampleRate::create(48000);
        QVERIFY(rate);
        auto good=frozen(44100,48000);
        auto opposite=frozen(48000,44100);
        QVERIFY(!good.empty()&&!opposite.empty());
        auto wrong=render::render_true_peak_limiter_owned_output_rate_candidate(
            owner,*manual.value(),frame_count(257),owner->chain_revision(),
            *rate.value(),*terminal.value(),binding,*registry.value(),
            block,{},nullptr,false,nullptr,opposite);
        QVERIFY(!wrong);
        good[good.size()/2]^=std::uint8_t{1};
        auto tampered=render::render_true_peak_limiter_owned_output_rate_candidate(
            owner,*manual.value(),frame_count(257),owner->chain_revision(),
            *rate.value(),*terminal.value(),binding,*registry.value(),
            block,{},nullptr,false,nullptr,good);
        QVERIFY(!tampered);
        auto stale=render::render_true_peak_limiter_owned_output_rate_candidate(
            owner,*manual.value(),frame_count(257),owner->chain_revision()+1,
            *rate.value(),*terminal.value(),binding,*registry.value(),
            block,{},nullptr,false,nullptr,opposite);
        QVERIFY(!stale);
        QCOMPARE(bits(owner->view()),before);
    }
};
} // namespace rgsml::tests

QTEST_APPLESS_MAIN(rgsml::tests::M22RealManualQ0M17ChainCandidateTest)
#include "test_m22_real_manual_q0_m17_chain_candidate.moc"
