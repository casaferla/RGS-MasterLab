// M22 NONRELEASE: actual canonical MANUAL DSP RenderResult -> frozen original
// Final SRC Q0 -> genuine bound M17 terminal/M16, without duplicate DSP.
#include "render_test_support.hpp"
#include "m22_full_chain_witness_candidate.hpp"
#include <rgsml/audio/phase11_final_src_candidate.hpp>
#include <rgsml/audio/full_resource_output_rate_candidate.hpp>
#include <rgsml/audio/internal/phase11_frozen_rgsmsr1_pins.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/true_peak_limiter_terminal_candidate.hpp>
#include <QtTest/QTest>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <numeric>
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
            for(const auto& witness:result.value()->terminal.checked.terminal.applied_gain_frames)
                QVERIFY(std::isfinite(witness.gain));
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
            QCOMPARE(bits(owner->view()),bitsBefore);
            // An identity clock must NOT consume or claim any FIR coefficient.
            {
                result.value()->src_kernel_sha256="forged-identity-coefficients";
                m22_private::FullChainWitnessFailure why{};
                QVERIFY(!m22_private::observe_real_complete_chain_candidate(
                    *result.value(),why));
                QCOMPARE(why,m22_private::FullChainWitnessFailure::INCORRECT_FROZEN_SRC_PROVENANCE);
                result.value()->src_kernel_sha256={};
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
