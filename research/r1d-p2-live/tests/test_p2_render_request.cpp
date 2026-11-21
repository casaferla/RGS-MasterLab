// I2.5 genuine RenderRequest parameters/ownership gate, no fabricated audio.
#include <rgsml/render/render_request.hpp>
#include <rgsml/audio/audio_buffer.hpp>
#include <rgsml/dsp/module_registry.hpp>
#include <rgsml/dsp/streaming_true_peak_limiter_parameters.hpp>
#include <rgsml/dsp/true_peak_limiter_parameters.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <rgsml/core/uuid.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace rgsml;
void require(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}
int main() {
    try {
        constexpr auto kP2="rgsml.dsp.streaming-true-peak-limiter";
        auto base=dsp::ModuleRegistry::create({});
        require(bool(base),"empty baseline registry");
        auto registry=dsp::ModuleRegistry::with_streaming_true_peak_preview(
            std::move(*base.value()));
        require(bool(registry),"opt-in Product registry");
        auto chain=dsp::ProcessingChain::create(*registry.value(),
            {dsp::ProcessingStage::MASTER,dsp::ChainSegment::MANUAL});
        require(bool(chain),"MANUAL chain");
        core::Uuid::Bytes bytes{};
        bytes[15]=5;
        auto id=dsp::ModuleInstanceId::from_uuid(core::Uuid{bytes});
        require(bool(id),"P2 ID");
        require(bool(chain.value()->add(*id.value(),kP2,0)),"insert P2");
        auto rate=core::SampleRate::create(48000);
        require(bool(rate),"rate");
        auto format=audio::AudioFormat::create(*rate.value(),
            audio::ChannelLayout::STEREO_LR);
        require(bool(format),"format");
        auto len=core::FrameCount::create(8192);
        auto block=core::FrameCount::create(512);
        require(bool(len)&&bool(block),"sizes");
        auto source=audio::AudioBuffer::create(*format.value(),
            audio::FrameDomainId::SOURCE_PROCESSING_RATE,
            core::FrameIndex{137},*len.value());
        require(bool(source),"source");
        auto window=core::FrameRange::create(core::FrameIndex{137},
                                               core::FrameIndex{8329});
        require(bool(window),"window");
        auto params=dsp::StreamingTruePeakLimiterParameters::create(-1.0,5.0,2);
        require(bool(params),"parameters");
        auto makeBindings=[&](){
            return std::vector<dsp::ModuleExecutionBinding>{
                dsp::ModuleExecutionBinding{*id.value(),
                    dsp::ModuleParameterPayload{*params.value()}}};
        };
        auto good=render::RenderRequest::create(
            source.value()->view(),*window.value(),*chain.value(),makeBindings(),
            *block.value());
        require(bool(good),"real P2 typed RenderRequest rejected");
        auto missing=render::RenderRequest::create(
            source.value()->view(),*window.value(),*chain.value(),{},
            *block.value());
        require(!missing,"P2 missing binding accepted");
        auto oldM17=dsp::TruePeakLimiterParameters::create_default();
        require(bool(oldM17),"M17 parameters");
        std::vector<dsp::ModuleExecutionBinding> wrong{{
            *id.value(),dsp::ModuleParameterPayload{*oldM17.value()}}};
        auto mixed=render::RenderRequest::create(
            source.value()->view(),*window.value(),*chain.value(),wrong,
            *block.value());
        require(!mixed,"M17 payload accepted for P2");
        auto duplicate=makeBindings();
        duplicate.push_back(duplicate.front());
        auto duplicated=render::RenderRequest::create(
            source.value()->view(),*window.value(),*chain.value(),duplicate,
            *block.value());
        require(!duplicated,"duplicate P2 binding accepted");
        require(bool(chain.value()->set_user_bypass(*id.value(),true)),
            "bypass");
        auto bypass=render::RenderRequest::create(
            source.value()->view(),*window.value(),*chain.value(),makeBindings(),
            *block.value());
        require(bool(bypass),"bypassed P2 needs authentic binding");
        auto bypassMissing=render::RenderRequest::create(
            source.value()->view(),*window.value(),*chain.value(),{},
            *block.value());
        require(!bypassMissing,"bypassed P2 omitted typed binding");
        std::cout << "I2 real RenderRequest P2 typed/bypass/duplicate/mismatch PASS" << std::endl;
        return 0;
    } catch(const std::exception& e) {
        std::cerr << "FAIL " << e.what() << std::endl;
        return 1;
    }
}
