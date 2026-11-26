#pragma once
// One local capture for one synchronous, complete REF_LINKED render.
// This is not yet an accepted playback sidecar. No data escapes unless
// every frame of an individual module was observed and the caller seals it.
#include <rgsml/render/linked_dsp_activity_sink_provider.hpp>
#include <rgsml/render/transient_activity_collector.hpp>
#include <rgsml/render/transient_c00_activity_sink.hpp>
#include <rgsml/render/dynamic_eq_activity_history.hpp>
#include <rgsml/dsp/processing_chain.hpp>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace rgsml::render::linked_activity {

class LinkedActivityRenderCapture final:public LinkedDspActivitySinkProvider {
    struct TransientEntry final {
        dsp::ModuleInstanceId id;
        TransientActivityCollector collector;
        TransientC00ActivitySink sink;
        TransientEntry(dsp::ModuleInstanceId node,std::uint32_t fs,std::size_t cap)
            :id(node),collector(fs,0,cap),sink(collector){}
    };
    struct DynamicEntry final {
        dsp::ModuleInstanceId id;
        C01ActivityHistoryCollector collector;
        DynamicEntry(dsp::ModuleInstanceId node,std::uint32_t fs,std::size_t cap)
            :id(node),collector(fs,0,cap){}
    };
public:
    // Budget the actual bucket storage across ALL linked instances. Long
    // tracks and allocation failures may omit Activity but never change PCM.
    explicit LinkedActivityRenderCapture(const dsp::ProcessingChain& chain,
        std::uint32_t fs,std::uint64_t total_frames,
        std::size_t max_bytes=128U*1024U*1024U) noexcept
        :revision_(chain.revision()),frames_(total_frames),fs_(fs)
    {
        if(chain.context()!=dsp::ProcessingChainContext{
            dsp::ProcessingStage::MASTER,dsp::ChainSegment::REF_LINKED}||
            total_frames==0||!(fs==44100U||fs==48000U||fs==88200U||
                fs==96000U||fs==176400U||fs==192000U))return;
        const auto step=fs/50U;
        const auto count=total_frames/step+(total_frames%step!=0?1U:0U);
        if(count>60000U)return;
        const auto cap=static_cast<std::size_t>(count);
        auto remaining=std::min(max_bytes,std::size_t{128U*1024U*1024U});
        for(const auto& node:chain.instances()) {
            if(!node.active())continue;
            try {
                if(node.module_type_id()=="rgsml.dsp.dynamic-eq") {
                    const auto bytes=cap*sizeof(C01ActivityBucket);
                    if(bytes>remaining)continue;
                    dynamic_.push_back(std::make_unique<DynamicEntry>(node.instance_id(),fs,cap));
                    remaining-=bytes;
                } else if(node.module_type_id()=="rgsml.dsp.transient-shaper") {
                    const auto bytes=cap*sizeof(AppliedGainBucket);
                    if(bytes>remaining)continue;
                    transient_.push_back(std::make_unique<TransientEntry>(node.instance_id(),fs,cap));
                    remaining-=bytes;
                }
            } catch(...) { /* optional evidence only */ }
        }
    }
    LinkedActivityRenderCapture(const LinkedActivityRenderCapture&)=delete;
    LinkedActivityRenderCapture& operator=(const LinkedActivityRenderCapture&)=delete;

    [[nodiscard]] dsp::internal::IC01ActivitySink*
    dynamic_eq_sink(const dsp::ModuleInstanceId& id) noexcept override {
        if(sealed_)return nullptr;
        for(auto& e:dynamic_)if(e->id==id)return &e->collector;
        return nullptr;
    }
    [[nodiscard]] dsp::internal::ITransientC00ActivitySink*
    transient_shaper_sink(const dsp::ModuleInstanceId& id) noexcept override {
        if(sealed_)return nullptr;
        for(auto& e:transient_)if(e->id==id)return &e->sink;
        return nullptr;
    }

    // Call only after all native stages and their provenance checks pass.
    // Incomplete observations are dropped, even when PCM itself succeeded.
    void seal_after_success() noexcept {
        if(sealed_)return;
        sealed_=true;
        std::erase_if(dynamic_,[&](const auto& e){
            return e->collector.next_frame()!=frames_||!e->collector.finish();
        });
        std::erase_if(transient_,[&](const auto& e){
            return e->sink.observation_failed()||e->collector.next_frame()!=frames_||
                !e->collector.finish();
        });
    }
    [[nodiscard]] std::vector<C01ActivityBucket> dynamic_window(
        const dsp::ModuleInstanceId& id,std::uint64_t frame,std::uint64_t window) const {
        if(sealed_&&frame<=frames_)for(const auto& e:dynamic_)if(e->id==id)
            return e->collector.visible_window(frame,window);
        return {};
    }
    [[nodiscard]] std::vector<AppliedGainBucket> transient_window(
        const dsp::ModuleInstanceId& id,std::uint64_t frame,std::uint64_t window) const {
        if(sealed_&&frame<=frames_)for(const auto& e:transient_)if(e->id==id)
            return e->collector.visible_window(frame,window);
        return {};
    }
    [[nodiscard]] std::vector<C01ActivityBucket> dynamic_history(
        const dsp::ModuleInstanceId& id) const {
        return dynamic_window(id,frames_,frames_);
    }
    [[nodiscard]] std::vector<AppliedGainBucket> transient_history(
        const dsp::ModuleInstanceId& id) const {
        return transient_window(id,frames_,frames_);
    }
    [[nodiscard]] bool sealed() const noexcept{return sealed_;}
    [[nodiscard]] std::uint64_t revision() const noexcept{return revision_;}
    [[nodiscard]] std::uint64_t frame_count() const noexcept{return frames_;}
    [[nodiscard]] std::uint32_t sample_rate_hz() const noexcept{return fs_;}
private:
    std::uint64_t revision_{},frames_{};
    std::uint32_t fs_{};
    bool sealed_{false};
    std::vector<std::unique_ptr<TransientEntry>> transient_;
    std::vector<std::unique_ptr<DynamicEntry>> dynamic_;
};
} // namespace rgsml::render::linked_activity
