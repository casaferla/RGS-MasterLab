#include "internal/l3_automatch_typed_gain_trial.hpp"
#include "internal/l3_automatch_typed_transient_trial.hpp"
#include "internal/l3_product_offline_trial_job.hpp"
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <QtTest/QTest>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>
#include <utility>

namespace rgsml::tests {
namespace {
namespace a=rgsml::app::l3_automatch;
using namespace rgsml;
[[nodiscard]] dsp::ModuleInstanceId id(const char* x) {
    auto parsed=core::Uuid::parse(x);
    Q_ASSERT(parsed);
    auto result=dsp::ModuleInstanceId::from_uuid(*parsed.value());
    Q_ASSERT(result);
    return *result.value();
}
class TransientProductTest final:public QObject {
    Q_OBJECT
private slots:
    void frozenRegistryOptInAndCanonicalRender();
    void nonPublishingProductCandidateUsesExistingM03();
};
void TransientProductTest::frozenRegistryOptInAndCanonicalRender() {
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    QVERIFY(!base.value()->has_factory("rgsml.dsp.transient-shaper"));
    const auto original=base.value()->find_descriptor(
        "rgsml.dsp.transient-shaper");
    QVERIFY(original);
    const auto count=base.value()->descriptors().size();
    auto opt=dsp::ModuleRegistry::with_transient_shaper_offline(
        std::move(*base.value()));
    QVERIFY(opt);
    QCOMPARE(opt.value()->descriptors().size(),count);
    QVERIFY(opt.value()->has_factory("rgsml.dsp.transient-shaper"));
    QVERIFY(!opt.value()->create_module("rgsml.dsp.transient-shaper"));
    QVERIFY(!dsp::ModuleRegistry::with_transient_shaper_offline(
        *opt.value()));

    auto chain=dsp::ProcessingChain::create(*opt.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto trans=id("a1000000-0000-4000-8000-000000000001");
    QVERIFY(chain.value()->add(trans,"rgsml.dsp.transient-shaper",0));
    auto p=dsp::TransientShaperParameters::create(10.0,-3.0);
    QVERIFY(p);
    const std::vector<dsp::ModuleExecutionBinding> bindings{
        {trans,*p.value()}};
    const auto format=audio::AudioFormat::create(
        *core::SampleRate::create(48000).value(),audio::ChannelLayout::MONO_C);
    const auto frames=core::FrameCount::create(4096);
    const auto block=core::FrameCount::create(256);
    QVERIFY(format&&frames&&block);
    auto inputBuffer=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(inputBuffer);
    auto pcm=inputBuffer.value()->mutable_view().channel(0);
    QVERIFY(pcm);
    for(std::size_t i=0;i<pcm.value()->size();++i) {
        const auto time=static_cast<double>(i);
        (*pcm.value())[i]=0.3*std::sin(
            2.0*std::numbers::pi*440.0*time/48000.0);
    }
    auto r=render::RenderRequest::create(
        inputBuffer.value()->view(),inputBuffer.value()->view().absolute_range(),
        *chain.value(),bindings,*block.value());
    QVERIFY(r);
    auto rendered=render::render_preview(*r.value(),*opt.value());
    QVERIFY(rendered);
    QCOMPARE(rendered.value()->signatures().size(),std::size_t{1});
    QVERIFY(std::holds_alternative<
        render::TransientShaperExecutionSignaturePayload>(
        rendered.value()->signatures()[0].payload));
    const auto signature=std::get<
        render::TransientShaperExecutionSignaturePayload>(
        rendered.value()->signatures()[0].payload);
    QCOMPARE(signature.attack_amount_db,10.0);
    QCOMPARE(signature.sustain_amount_db,-3.0);
    auto output=rendered.value()->view().channel(0);
    QVERIFY(output);
    bool changed=false;
    for(std::size_t i=0;i<pcm.value()->size();++i)
        if(std::bit_cast<std::uint64_t>((*pcm.value())[i])!=
           std::bit_cast<std::uint64_t>((*output.value())[i])) {
            changed=true;break;
        }
    QVERIFY(changed);
    const auto partial=core::FrameRange::create(
        core::FrameIndex{256},core::FrameIndex{1024});
    QVERIFY(partial);
    auto crop=render::RenderRequest::create(
        inputBuffer.value()->view(),*partial.value(),*chain.value(),
        bindings,*block.value());
    QVERIFY(crop);
    QVERIFY(!render::render_preview(*crop.value(),*opt.value()));
    QVERIFY(chain.value()->set_user_bypass(trans,true));
    auto bypassed=render::RenderRequest::create(
        inputBuffer.value()->view(),*partial.value(),*chain.value(),
        bindings,*block.value());
    QVERIFY(bypassed);
    auto bypassResult=render::render_preview(*bypassed.value(),*opt.value());
    QVERIFY(bypassResult);
    QVERIFY(bypassResult.value()->signatures()[0].disposition==
        render::ModuleExecutionDisposition::BYPASS_IDENTITY);
    const auto wrong=dsp::GainParameters::create(0.0);
    QVERIFY(wrong);
    QVERIFY(!render::RenderRequest::create(
        inputBuffer.value()->view(),inputBuffer.value()->view().absolute_range(),
        *chain.value(),{{trans,*wrong.value()}},*block.value()));
    QVERIFY(!render::RenderRequest::create(
        inputBuffer.value()->view(),inputBuffer.value()->view().absolute_range(),
        *chain.value(),{},*block.value()));
}
void TransientProductTest::nonPublishingProductCandidateUsesExistingM03() {
    auto created=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(created);
    auto product=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*created.value()));
    QVERIFY(!product->has_factory("rgsml.dsp.transient-shaper"));
    auto chain=dsp::ProcessingChain::create(*product,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    auto empty=dsp::ProcessingChain::create(*product,{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain&&empty);
    const auto gainId=id("a2000000-0000-4000-8000-000000000001");
    const auto m03=id("a2000000-0000-4000-8000-000000000002");
    QVERIFY(chain.value()->add(gainId,"rgsml.dsp.gain",0));
    QVERIFY(chain.value()->add(m03,"rgsml.dsp.transient-shaper",1));
    auto gain=dsp::GainParameters::create(0.0);
    auto transient=dsp::TransientShaperParameters::create(8.0,5.0);
    QVERIFY(gain&&transient);
    const std::vector<dsp::ModuleExecutionBinding> baseline{
        {gainId,*gain.value()},{m03,*transient.value()}};
    auto proposal=a::make_gain_trial(
        *chain.value(),baseline,gainId,3.0,"gain-plus-transient-m03");
    QVERIFY(proposal);
    const auto format=audio::AudioFormat::create(
        *core::SampleRate::create(48000).value(),audio::ChannelLayout::MONO_C);
    const auto length=core::FrameCount::create(4096);
    const auto block=core::FrameCount::create(256);
    QVERIFY(format&&length&&block);
    auto source=audio::AudioBuffer::create(*format.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*length.value());
    QVERIFY(source);
    auto input=source.value()->mutable_view().channel(0);
    QVERIFY(input);
    for(std::size_t i=0;i<input.value()->size();++i)
        (*input.value())[i]=0.3*std::sin(
            2.0*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
    auto request=render::RenderRequest::create(
        source.value()->view(),source.value()->view().absolute_range(),
        *empty.value(),{},*block.value());
    QVERIFY(request);
    auto prepared=render::render_preview(*request.value(),*product);
    QVERIFY(prepared);
    auto owner=std::make_shared<const render::RenderResult>(
        std::move(*prepared.value()));
    a::OfflineProductOwnerStamp stamp{
        54,7,2,"test.provider","source-project-54",
        "test.provider","ref-picker-2"};
    auto job=a::OfflineProductTrialJob::create(
        stamp,owner,product,*chain.value(),baseline,*proposal,*block.value());
    QVERIFY(job);
    auto calculated=job.value()->evaluate();
    QVERIFY(calculated);
    QVERIFY(job.value()->result_has_expected_scope(
        calculated.value()->rendered));
    QCOMPARE(calculated.value()->rendered.signatures().size(),
             std::size_t{2});
    QVERIFY(std::holds_alternative<
        render::TransientShaperExecutionSignaturePayload>(
        calculated.value()->rendered.signatures()[1].payload));

    // An independent typed Attack-only candidate crosses the SAME actual
    // RenderPreview/M19/M20 worker path, and Sustain remains unchanged.
    auto attack=a::make_transient_scalar_trial(
        *chain.value(),baseline,m03,
        a::TransientCoordinate::attack_amount_db,
        10.0,"attack-10-sustain-5");
    QVERIFY(attack);
    auto* value=std::get_if<dsp::TransientShaperParameters>(
        &attack->candidate_bindings[1].parameters);
    QVERIFY(value);
    QCOMPARE(value->attack_amount_db(),10.0);
    QCOMPARE(value->sustain_amount_db(),5.0);
    QVERIFY(attack->candidate_bindings[0].parameters==
            baseline[0].parameters);
    QVERIFY(!a::make_transient_scalar_trial(
        *chain.value(),baseline,m03,
        a::TransientCoordinate::attack_amount_db,
        8.0,"no-op"));
    QVERIFY(!a::make_transient_scalar_trial(
        *chain.value(),baseline,m03,
        a::TransientCoordinate::sustain_amount_db,
        18.0,"out-of-range"));
    auto altJob=a::OfflineProductTrialJob::create(
        stamp,owner,job.value()->registry_snapshot(),
        *chain.value(),baseline,*attack,*block.value());
    QVERIFY(altJob);
    auto alt=altJob.value()->evaluate();
    QVERIFY(alt);
    QVERIFY(altJob.value()->result_has_expected_scope(alt.value()->rendered));
    QCOMPARE(alt.value()->rendered.signatures().size(),std::size_t{2});
    QVERIFY(alt.value()->rendered.signatures()[0].payload!=
            calculated.value()->rendered.signatures()[0].payload);
    QVERIFY(alt.value()->rendered.signatures()[1].payload!=
            calculated.value()->rendered.signatures()[1].payload);
    QVERIFY(!product->has_factory("rgsml.dsp.transient-shaper"));
    QCOMPARE(chain.value()->instances().size(),std::size_t{2});
    auto changed=stamp;
    changed.reference_selection_revision++;
    QVERIFY(!job.value()->still_current(
        changed,owner,*chain.value(),baseline));
    auto bindings=baseline;
    bindings[1].parameters=*dsp::TransientShaperParameters::create(
        8.0,6.0).value();
    QVERIFY(!job.value()->still_current(
        stamp,owner,*chain.value(),bindings));
}
} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::TransientProductTest)
#include "test_l3_transient_shaper_product.moc"
