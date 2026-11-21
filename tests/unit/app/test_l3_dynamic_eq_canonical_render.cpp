#include "internal/m21_real_product_metrics_port.hpp"
#include "internal/l2_automatch_evidence_bridge.hpp"
#include "internal/l3_automatch_offline_search.hpp"
#include "internal/l3_automatch_typed_gain_trial.hpp"
#include "internal/l3_product_offline_trial_job.hpp"
#include "internal/l3_offline_private_metric_probe.hpp"
#include <rgsml/render/render_request.hpp>
#include <rgsml/render/render_preview.hpp>
#include <rgsml/render/render_result.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <rgsml/dsp/gain_parameters.hpp>
#include <rgsml/analysis/basic_level_analyzer.hpp>
#include <rgsml/analysis/tonal_stereo_analyzer.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <QtTest/QTest>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace rgsml::tests {
namespace {
using namespace rgsml;

[[nodiscard]] dsp::ModuleInstanceId id(const char* value) {
    auto u=core::Uuid::parse(value);
    Q_ASSERT(u);
    auto v=dsp::ModuleInstanceId::from_uuid(*u.value());
    Q_ASSERT(v);
    return *v.value();
}
[[nodiscard]] dsp::DynamicEqParameters dynamic_params(double threshold=-80.0) {
    dsp::DynamicEqBandSpec s;
    s.band_id=*core::Uuid::parse(
        "aaaabbbb-1111-4111-8111-444455556666").value();
    s.filter_type=dsp::DynamicEqFilterType::BELL;
    s.shape=dsp::DynamicEqBellShape{440.0,0.707};
    s.routing=dsp::EqRouting::STEREO;
    s.control.threshold_dbfs=threshold;
    s.control.ratio=20.0;
    s.control.range_db=6.0;
    s.control.rms_time_constant_ms=1.0;
    s.control.attack_ms=0.1;
    s.control.release_ms=10.0;
    auto band=dsp::DynamicEqBandParameters::create(s);
    Q_ASSERT(band);
    auto params=dsp::DynamicEqParameters::create({*band.value()});
    Q_ASSERT(params);
    return *params.value();
}
class L3CanonicalC01RenderTest final:public QObject {
    Q_OBJECT
private slots:
    void wholeProgrammePhysicalRendererAndM21Scoring();
    void regionAndUnboundParametersFailClosed();
    void signatureExcludesDisabledAndUuidIdentity();
    void immutableProductPreparedCandidateAndOwnerEpochGate();
};

void L3CanonicalC01RenderTest::wholeProgrammePhysicalRendererAndM21Scoring() {
    using namespace app::m21_private;
    using namespace app::l3_automatch;
    qInfo() << "L3_SCOPE full-programme: entry";
    auto base=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(base);
    QVERIFY(!base.value()->has_factory("rgsml.dsp.dynamic-eq"));
    // The frozen L1 catalog already contains the Dynamic EQ *descriptor*.
    // The opt-in must activate this exact slot, not introduce a duplicate.
    const auto prior=base.value()->find_descriptor("rgsml.dsp.dynamic-eq");
    QVERIFY(prior);
    const auto descriptor_count=base.value()->descriptors().size();
    const auto prior_categories=std::vector<dsp::ModuleCategory>(
        prior.value()->get().categories().begin(),
        prior.value()->get().categories().end());
    auto opt=dsp::ModuleRegistry::with_dynamic_eq_offline(std::move(*base.value()));
    QVERIFY(opt);
    QVERIFY(opt.value()->has_factory("rgsml.dsp.dynamic-eq"));
    QCOMPARE(opt.value()->descriptors().size(),descriptor_count);
    const auto activated=opt.value()->find_descriptor("rgsml.dsp.dynamic-eq");
    QVERIFY(activated);
    QVERIFY(std::equal(activated.value()->get().categories().begin(),
                       activated.value()->get().categories().end(),
                       prior_categories.begin(),prior_categories.end()));
    QVERIFY(!opt.value()->create_module("rgsml.dsp.dynamic-eq"));

    auto chain=dsp::ProcessingChain::create(*opt.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto gain_id=id("a0000000-0000-4000-8000-000000000001");
    const auto eq_id=id("a0000000-0000-4000-8000-000000000002");
    QVERIFY(chain.value()->add(gain_id,"rgsml.dsp.gain",0));
    QVERIFY(chain.value()->add(eq_id,"rgsml.dsp.dynamic-eq",1));
    qInfo() << "L3_SCOPE full-programme: chain instantiated";
    const auto gain=dsp::GainParameters::create(0.0);
    QVERIFY(gain);
    const auto eq=dynamic_params();
    const std::vector<dsp::ModuleExecutionBinding> bindings{
        {gain_id,*gain.value()},{eq_id,eq}};

    const auto fs=core::SampleRate::create(48000);
    const auto n=core::FrameCount::create(48000);
    const auto block=core::FrameCount::create(1024);
    QVERIFY(fs&&n&&block);
    const auto fmt=audio::AudioFormat::create(*fs.value(),
        audio::ChannelLayout::MONO_C);
    QVERIFY(fmt);
    auto src=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},
        *n.value());
    auto reference=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},
        *n.value());
    QVERIFY(src&&reference);
    const auto source_samples=src.value()->mutable_view().channel(0);
    const auto target_samples=reference.value()->mutable_view().channel(0);
    QVERIFY(source_samples&&target_samples);
    for(std::size_t i=0;i<source_samples.value()->size();++i) {
        const double t=static_cast<double>(i);
        const double wave=std::sin(2.0*std::numbers::pi*440.0*t/48000.0);
        (*source_samples.value())[i]=0.2*wave;
        (*target_samples.value())[i]=0.4*wave;
    }
    QVERIFY(chain.value()->set_user_bypass(eq_id,true));
    auto baseline_request=render::RenderRequest::create(
        src.value()->view(),src.value()->view().absolute_range(),
        *chain.value(),bindings,*block.value());
    QVERIFY(baseline_request);
    auto baseline=render::render_preview(*baseline_request.value(),*opt.value());
    QVERIFY(baseline);
    qInfo() << "L3_SCOPE full-programme: bypassed baseline rendered";
    QCOMPARE(baseline.value()->signatures().size(),std::size_t{2});
    const auto& signature0=baseline.value()->signatures()[1];
    QVERIFY(signature0.disposition==render::ModuleExecutionDisposition::BYPASS_IDENTITY);
    QVERIFY(std::holds_alternative<render::DynamicEqExecutionSignaturePayload>(
        signature0.payload));
    QCOMPARE(std::get<render::DynamicEqExecutionSignaturePayload>(
        signature0.payload).enabled_bands.size(),std::size_t{1});

    QVERIFY(chain.value()->set_user_bypass(eq_id,false));
    auto request=render::RenderRequest::create(
        src.value()->view(),src.value()->view().absolute_range(),
        *chain.value(),bindings,*block.value());
    QVERIFY(request);
    auto actual=render::render_preview(*request.value(),*opt.value());
    QVERIFY(actual);
    qInfo() << "L3_SCOPE full-programme: C01 active rendered";
    QCOMPARE(actual.value()->signatures().size(),std::size_t{2});
    const auto& signature=actual.value()->signatures()[1];
    QVERIFY(signature.disposition==render::ModuleExecutionDisposition::PROCESSED);
    QVERIFY(std::holds_alternative<render::DynamicEqExecutionSignaturePayload>(
        signature.payload));
    QVERIFY(signature.payload==signature0.payload);
    // One additional immutable typed Gain proposal now carries the new
    // Dynamic EQ stage without mutating the live chain or omitting bindings.
    auto gain_trial=make_gain_trial(*chain.value(),bindings,gain_id,3.0,
                                   "gain-3db-plus-dynamic-eq");
    QVERIFY(gain_trial);
    QVERIFY(gain_trial->still_matches(*chain.value(),bindings));
    QCOMPARE(gain_trial->candidate_bindings.size(),std::size_t{2});
    QVERIFY(gain_trial->candidate_bindings[1].parameters==bindings[1].parameters);
    auto gain_request=render::RenderRequest::create(
        src.value()->view(),src.value()->view().absolute_range(),
        *chain.value(),gain_trial->candidate_bindings,*block.value());
    QVERIFY(gain_request);
    auto gain_render=render::render_preview(*gain_request.value(),*opt.value());
    QVERIFY(gain_render);
    qInfo() << "L3_SCOPE full-programme: Gain plus C01 rendered";
    QCOMPARE(gain_render.value()->signatures().size(),std::size_t{2});
    QVERIFY(gain_render.value()->signatures()[1].payload==signature.payload);
    const auto gain_output=gain_render.value()->view().channel(0);
    const auto original_output=actual.value()->view().channel(0);
    QVERIFY(gain_output&&original_output);
    bool changed=false;
    for(std::size_t i=0;i<gain_output.value()->size();++i)
        if((*gain_output.value())[i]!=(*original_output.value())[i]) {
            changed=true;break;
        }
    QVERIFY(changed);
    const auto l_base=analysis::measure_basic_level_full_resource(
        baseline.value()->buffer());
    const auto l_actual=analysis::measure_basic_level_full_resource(
        actual.value()->buffer());
    const auto l_ref=analysis::measure_basic_level_full_resource(*reference.value());
    const auto t_base=analysis::measure_tonal_stereo_scope(
        baseline.value()->view(),core::FrameIndex{0},*n.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    const auto t_actual=analysis::measure_tonal_stereo_scope(
        actual.value()->view(),core::FrameIndex{0},*n.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    const auto t_ref=analysis::measure_tonal_stereo_scope(
        reference.value()->view(),core::FrameIndex{0},*n.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(l_base&&l_actual&&l_ref&&t_base&&t_actual&&t_ref);
    qInfo() << "L3_SCOPE full-programme: physical analysis ready";
    const auto baseMetrics=import_real_product_metrics(
        *l_base.value(),*t_base.value(),
        {Column::processed,1U,"actual-render-bypassed","Full Track",true});
    const auto trialMetrics=import_real_product_metrics(
        *l_actual.value(),*t_actual.value(),
        {Column::processed,2U,"actual-render-c01","Full Track",true});
    const auto refMetrics=import_real_product_metrics(
        *l_ref.value(),*t_ref.value(),
        {Column::gold,1U,"independent-reference","Full Track",true});
    QVERIFY(baseMetrics.snapshot&&trialMetrics.snapshot&&refMetrics.snapshot);
    PrivatePublicationGuard guard;
    QVERIFY(guard.expect(Column::processed,1U));
    QVERIFY(guard.expect(Column::gold,1U));
    QVERIFY(guard.publish(*baseMetrics.snapshot));
    QVERIFY(guard.publish(*refMetrics.snapshot));
    auto search=OfflineSearch::begin(5401,guard.view(),
        {{"typed-c01-bell-full-render"}},
        {{Metric::full_scope_rms,1.0,1.0}});
    QVERIFY(search);
    const auto ticket=search->next();
    QVERIFY(ticket);
    QVERIFY(guard.expect(Column::processed,2U));
    QVERIFY(guard.publish(*trialMetrics.snapshot));
    QVERIFY(search->complete(*ticket,guard.view())==Submission::improved);
    QVERIFY(search->best().typed_dsp_fingerprint);
    QCOMPARE(*search->best().typed_dsp_fingerprint,
             std::string{"typed-c01-bell-full-render"});
    QVERIFY(search->best().weighted_error<0.5);
    qInfo() << "L3_SCOPE full-programme: M21 candidate improved";
    QVERIFY(chain.value()->set_user_bypass(eq_id,true));
    QVERIFY(!gain_trial->still_matches(*chain.value(),bindings));
}

void L3CanonicalC01RenderTest::regionAndUnboundParametersFailClosed() {
    qInfo() << "L3_SCOPE region: entry";
    auto registry=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(registry);
    auto opt=dsp::ModuleRegistry::with_dynamic_eq_offline(std::move(*registry.value()));
    QVERIFY(opt);
    auto chain=dsp::ProcessingChain::create(*opt.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto eq_id=id("a0000000-0000-4000-8000-000000000002");
    QVERIFY(chain.value()->add(eq_id,"rgsml.dsp.dynamic-eq",0));
    const auto eq=dynamic_params();
    const auto bad_gain=dsp::GainParameters::create(0.0);
    QVERIFY(bad_gain);
    const auto fmt=audio::AudioFormat::create(
        *core::SampleRate::create(48000).value(),audio::ChannelLayout::MONO_C);
    const auto frames=core::FrameCount::create(4096);
    QVERIFY(fmt&&frames);
    auto src=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,core::FrameIndex{0},
        *frames.value());
    QVERIFY(src);
    QVERIFY(!render::RenderRequest::create(src.value()->view(),
        src.value()->view().absolute_range(),*chain.value(),
        {{eq_id,*bad_gain.value()}},*frames.value()));
    QVERIFY(!render::RenderRequest::create(src.value()->view(),
        src.value()->view().absolute_range(),*chain.value(),
        {},*frames.value()));
    const std::vector<dsp::ModuleExecutionBinding> bindings{{eq_id,eq}};
    const auto full=render::RenderRequest::create(src.value()->view(),
        src.value()->view().absolute_range(),*chain.value(),bindings,
        *frames.value());
    QVERIFY(full);
    QVERIFY(render::render_preview(*full.value(),*opt.value()));
    const auto region=core::FrameRange::create(
        core::FrameIndex{512},core::FrameIndex{2048});
    QVERIFY(region);
    auto cut=render::RenderRequest::create(src.value()->view(),*region.value(),
        *chain.value(),bindings,*frames.value());
    QVERIFY(cut);
    QVERIFY(!render::render_preview(*cut.value(),*opt.value()));
    QVERIFY(chain.value()->set_user_bypass(eq_id,true));
    auto bypass=render::RenderRequest::create(src.value()->view(),
        *region.value(),*chain.value(),bindings,*frames.value());
    QVERIFY(bypass);
    QVERIFY(render::render_preview(*bypass.value(),*opt.value()));
}
void L3CanonicalC01RenderTest::signatureExcludesDisabledAndUuidIdentity() {
    qInfo() << "L3_SCOPE identity: entry";
    auto original=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(original);
    auto registry=dsp::ModuleRegistry::with_dynamic_eq_offline(
        std::move(*original.value()));
    QVERIFY(registry);
    auto chain=dsp::ProcessingChain::create(*registry.value(),{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
    QVERIFY(chain);
    const auto eq_id=id("a0000000-0000-4000-8000-000000000004");
    QVERIFY(chain.value()->add(eq_id,"rgsml.dsp.dynamic-eq",0));
    const auto single=dynamic_params();
    auto changed=single.bands()[0].spec();
    changed.band_id=*core::Uuid::parse(
        "55555555-4444-4333-8222-111111111111").value();
    dsp::DynamicEqBandSpec disabled;
    disabled.band_id=*core::Uuid::parse(
        "22222222-3333-4444-8555-666666666666").value();
    disabled.routing=dsp::EqRouting::RIGHT;
    disabled.enabled=false;
    auto activeBand=dsp::DynamicEqBandParameters::create(changed);
    auto inactiveBand=dsp::DynamicEqBandParameters::create(disabled);
    QVERIFY(activeBand&&inactiveBand);
    auto extra=dsp::DynamicEqParameters::create(
        {*activeBand.value(),*inactiveBand.value()});
    QVERIFY(extra);
    const auto fmt=audio::AudioFormat::create(
        *core::SampleRate::create(48000).value(),audio::ChannelLayout::MONO_C);
    const auto count=core::FrameCount::create(4096);
    QVERIFY(fmt&&count);
    auto source=audio::AudioBuffer::create(*fmt.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*count.value());
    QVERIFY(source);
    auto one=render::RenderRequest::create(source.value()->view(),
        source.value()->view().absolute_range(),*chain.value(),
        {{eq_id,single}},*count.value());
    auto two=render::RenderRequest::create(source.value()->view(),
        source.value()->view().absolute_range(),*chain.value(),
        {{eq_id,*extra.value()}},*count.value());
    QVERIFY(one&&two);
    auto first=render::render_preview(*one.value(),*registry.value());
    auto second=render::render_preview(*two.value(),*registry.value());
    QVERIFY(first&&second);
    QCOMPARE(first.value()->signatures().size(),std::size_t{1});
    QCOMPARE(second.value()->signatures().size(),std::size_t{1});
    QVERIFY(first.value()->signatures()[0].payload==
            second.value()->signatures()[0].payload);
    const auto& a=std::get<render::DynamicEqExecutionSignaturePayload>(
        first.value()->signatures()[0].payload);
    QCOMPARE(a.enabled_bands.size(),std::size_t{1});
    const auto x=first.value()->view().channel(0);
    const auto y=second.value()->view().channel(0);
    QVERIFY(x&&y);
    QCOMPARE(x.value()->size(),y.value()->size());
    QVERIFY(std::equal(x.value()->begin(),x.value()->end(),
                       y.value()->begin()));
    // Opt-in is a one-time factory attachment, never additive registration.
    QVERIFY(!dsp::ModuleRegistry::with_dynamic_eq_offline(
        std::move(*registry.value())));
}

void L3CanonicalC01RenderTest::immutableProductPreparedCandidateAndOwnerEpochGate() {
    using namespace app::l3_automatch;
    // Product starts with frozen L1 catalog (factory-less Dynamic EQ).
    // Only the worker-owned copy activates C01: never change Product.
    auto p=dsp::ModuleRegistry::create_dsp_package_v1();
    QVERIFY(p);
    auto product=std::make_shared<const dsp::ModuleRegistry>(
        std::move(*p.value()));
    QVERIFY(!product->has_factory("rgsml.dsp.dynamic-eq"));
    const dsp::ProcessingChainContext context{
        dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL};
    auto empty=dsp::ProcessingChain::create(*product,context);
    auto manual=dsp::ProcessingChain::create(*product,context);
    QVERIFY(empty&&manual);
    const auto gainId=id("a0000000-0000-4000-8000-000000000011");
    const auto eqId=id("a0000000-0000-4000-8000-000000000012");
    QVERIFY(manual.value()->add(gainId,"rgsml.dsp.gain",0));
    QVERIFY(manual.value()->add(eqId,"rgsml.dsp.dynamic-eq",1));
    auto defaultGain=dsp::GainParameters::create(0.0);
    QVERIFY(defaultGain);
    const auto eq=dynamic_params();
    const std::vector<dsp::ModuleExecutionBinding> baseline{
        {gainId,*defaultGain.value()},{eqId,eq}};
    auto bound=make_gain_trial(*manual.value(),baseline,gainId,3.0,
                               "product-gain-and-c01-epoch-1");
    QVERIFY(bound);
    const auto format=audio::AudioFormat::create(
        *core::SampleRate::create(48000).value(),audio::ChannelLayout::MONO_C);
    const auto frames=core::FrameCount::create(4096);
    const auto block=core::FrameCount::create(1024);
    QVERIFY(format&&frames&&block);
    auto raw=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(raw);
    auto data=raw.value()->mutable_view().channel(0);
    QVERIFY(data);
    for(std::size_t i=0;i<data.value()->size();++i)
        (*data.value())[i]=0.2*std::sin(
            2*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
    // A real empty-chain RenderPreview is the retained immutable PREPARED.
    auto prepareRequest=render::RenderRequest::create(
        raw.value()->view(),raw.value()->view().absolute_range(),
        *empty.value(),{},*block.value());
    QVERIFY(prepareRequest);
    auto preparedRender=render::render_preview(
        *prepareRequest.value(),*product);
    QVERIFY(preparedRender);
    auto prepared=std::make_shared<const render::RenderResult>(
        std::move(*preparedRender.value()));

    const OfflineProductOwnerStamp stamp{
        19,5,3,"test.provider","source:actual-owner-5",
        "test.provider","reference:actual-picker-3"};
    auto job=OfflineProductTrialJob::create(
        stamp,prepared,product,*manual.value(),baseline,*bound,*block.value());
    QVERIFY(job);
    QVERIFY(job.value()->still_current(stamp,prepared,*manual.value(),baseline));
    QCOMPARE(job.value()->ticket_identity(),
             std::string("product-gain-and-c01-epoch-1"));
    const auto candidate=job.value()->evaluate();
    QVERIFY(candidate);
    QVERIFY(job.value()->result_has_expected_scope(candidate.value()->rendered));
    QVERIFY(candidate.value()->levels.scope_frame_count==4096);
    QVERIFY(candidate.value()->tonal.scope_frame_count==4096);

    // INDEPENDENT measured reference. The fixture grants only a private
    // test owner claim; no Product/QML publication is performed by probe.
    auto goldBuffer=audio::AudioBuffer::create(
        *format.value(),audio::FrameDomainId::SOURCE_PROCESSING_RATE,
        core::FrameIndex{0},*frames.value());
    QVERIFY(goldBuffer);
    auto goldSamples=goldBuffer.value()->mutable_view().channel(0);
    QVERIFY(goldSamples);
    for(std::size_t i=0;i<goldSamples.value()->size();++i)
        (*goldSamples.value())[i]=0.4*std::sin(
            2*std::numbers::pi*440.0*static_cast<double>(i)/48000.0);
    auto goldLevels=analysis::measure_basic_level_full_resource(
        *goldBuffer.value());
    auto goldTonal=analysis::measure_tonal_stereo_scope(
        goldBuffer.value()->view(),core::FrameIndex{0},*frames.value(),
        audio::FrameDomainId::SOURCE_PROCESSING_RATE);
    QVERIFY(goldLevels&&goldTonal);
    const auto imported=app::m21_private::import_real_product_metrics(
        *goldLevels.value(),*goldTonal.value(),
        {app::m21_private::Column::gold,23,
         "independent-test-gold-real-pcm","Full Track",true});
    QVERIFY(imported.snapshot);
    const std::array<Objective,1> rms{{
        {app::m21_private::Metric::full_scope_rms,1.0,1.0}}};
    const auto probe=probe_unpublished_candidate(
        *candidate.value(),*imported.snapshot,rms);
    QVERIFY(probe);
    QVERIFY(std::isfinite(probe.value()->normalized_weighted_squared_error));
    QCOMPARE(probe.value()->evaluated_metric_count,std::size_t{1});
    QCOMPARE(probe.value()->candidate_frames,4096);
    const std::array<Objective,1> m16tp{{
        {app::m21_private::Metric::true_peak,1.0,1.0}}};
    QVERIFY(!probe_unpublished_candidate(
        *candidate.value(),*imported.snapshot,m16tp));
    const std::array<Objective,2> duplicate{{
        {app::m21_private::Metric::full_scope_rms,1.0,1.0},
        {app::m21_private::Metric::full_scope_rms,1.0,1.0}}};
    QVERIFY(!probe_unpublished_candidate(
        *candidate.value(),*imported.snapshot,duplicate));
    auto staleReference=*imported.snapshot;
    staleReference.lifecycle=app::m21_private::Lifecycle::stale;
    QVERIFY(!probe_unpublished_candidate(
        *candidate.value(),staleReference,rms));
    QCOMPARE(candidate.value()->rendered.signatures().size(),std::size_t{2});
    QVERIFY(std::holds_alternative<render::DynamicEqExecutionSignaturePayload>(
        candidate.value()->rendered.signatures()[1].payload));
    QVERIFY(!product->has_factory("rgsml.dsp.dynamic-eq"));
    QCOMPARE(manual.value()->instances().size(),std::size_t{2});

    // Replacing immutable PREPARED PCM without advancing a coincidentally
    // equal owner epoch must STILL invalidate the worker's authorization.
    auto secondPrepare=render::render_preview(*prepareRequest.value(),*product);
    QVERIFY(secondPrepare);
    auto otherPrepared=std::make_shared<const render::RenderResult>(
        std::move(*secondPrepare.value()));
    QVERIFY(!job.value()->still_current(
        stamp,otherPrepared,*manual.value(),baseline));

    // Each negative case tests its own missing authority while the exact
    // live chain still matches the original validated trial.
    auto noEpoch=stamp;
    noEpoch.m18_owner_epoch=0;
    QVERIFY(!OfflineProductTrialJob::create(
        noEpoch,prepared,product,*manual.value(),baseline,*bound,*block.value()));
    QVERIFY(!OfflineProductTrialJob::create(
        stamp,{},product,*manual.value(),baseline,*bound,*block.value()));
    QVERIFY(!OfflineProductTrialJob::create(
        stamp,prepared,{},*manual.value(),baseline,*bound,*block.value()));
    const auto wrongBinding=std::vector<dsp::ModuleExecutionBinding>{
        {gainId,eq},{eqId,*defaultGain.value()}};
    QVERIFY(!OfflineProductTrialJob::create(
        stamp,prepared,product,*manual.value(),wrongBinding,*bound,*block.value()));

    auto stale=stamp;
    stale.m18_owner_epoch++;
    QVERIFY(!job.value()->still_current(stale,prepared,*manual.value(),baseline));
    stale=stamp;stale.source_generation++;
    QVERIFY(!job.value()->still_current(stale,prepared,*manual.value(),baseline));
    stale=stamp;stale.reference_selection_revision++;
    QVERIFY(!job.value()->still_current(stale,prepared,*manual.value(),baseline));
    stale=stamp;stale.reference_locator+="another-selection";
    QVERIFY(!job.value()->still_current(stale,prepared,*manual.value(),baseline));
    auto changedBindings=baseline;
    changedBindings[0].parameters=*dsp::GainParameters::create(1.0).value();
    QVERIFY(!job.value()->still_current(stamp,prepared,*manual.value(),changedBindings));
    QVERIFY(manual.value()->set_user_bypass(eqId,true));
    QVERIFY(!job.value()->still_current(stamp,prepared,*manual.value(),baseline));
}

} // namespace
} // namespace rgsml::tests
QTEST_GUILESS_MAIN(rgsml::tests::L3CanonicalC01RenderTest)
#include "test_l3_dynamic_eq_canonical_render.moc"
