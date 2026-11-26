#pragma once
// Product-owned, read-only REF_LINKED adapter. No shadow DSP or persistent
// PCM owner. The shared host can retain an adapter across a UI transition,
// but every parameter read must recheck the actual current Product graph.
#include "dsp_module_adapter.hpp"
#include "l3_linked_editor_parameter_projection.hpp"
#include "l3_linked_activity_product_projection.hpp"
#include <QPointer>
#include <string>

namespace rgsml::app::l3_automatch {
class LinkedDspModuleAdapter final : public DspModuleAdapter {
public:
    LinkedDspModuleAdapter(const LinkedEditorSnapshot& snapshot,
        MasteringPreviewController& preview,MasteringChainState& manual,
        QString context,QObject* parent=nullptr)
        :DspModuleAdapter(parent),preview_(&preview),manual_(&manual),
         exact_owner_(snapshot.exact_owner),id_(snapshot.instance.instance_id()),
         type_(snapshot.instance.module_type_id()),context_(std::move(context))
    {
        connect(&preview,&MasteringPreviewController::changed,
            this,&DspModuleAdapter::changed);
    }

    void bind_activity(AuditionSourceSelector* selector,PlaybackTransportViewModel* transport)
    {
        selector_=selector;
        transport_=transport;
        if(selector)connect(selector,&AuditionSourceSelector::changed,this,&DspModuleAdapter::activityChanged);
        if(transport)connect(transport,&PlaybackTransportViewModel::playbackChanged,this,&DspModuleAdapter::activityChanged);
        connect(preview_.data(),&MasteringPreviewController::changed,this,&DspModuleAdapter::activityChanged);
    }
    [[nodiscard]] QVariantMap linked_activity() const override
    {
        const auto s=current();
        if(!s||!selector_||!transport_)return {};
        return project_linked_activity_qt(resolve_linked_activity(*s,*preview_,*selector_,*transport_));
    }

    [[nodiscard]] QString instance_id() const override
    {return QString::fromStdString(id_.to_string());}
    [[nodiscard]] QString type_id() const override
    {return QString::fromStdString(type_);}
    [[nodiscard]] QString display_name() const override
    {return dynamic()?QStringLiteral("Dynamic EQ"):QStringLiteral("Transient Shaper");}
    [[nodiscard]] QString workspace_context_label() const override {return context_;}
    [[nodiscard]] QString configuration_state() const override
    {return current()?QStringLiteral("Reference Linked"):QStringLiteral("Unavailable");}
    [[nodiscard]] bool bypass_supported() const noexcept override {return false;}
    [[nodiscard]] bool bypass() const noexcept override
    {const auto s=current();return !s||s->instance.user_bypass();}
    [[nodiscard]] bool preview_supported() const noexcept override {return false;}
    [[nodiscard]] QString preview_status() const override
    {return current()?QStringLiteral("REF_LINKED_READ_ONLY"):QStringLiteral("UNAVAILABLE");}
    [[nodiscard]] QString preview_error() const override
    {return current()?QString{}:QStringLiteral("Linked DSP owner no longer current");}
    [[nodiscard]] bool history_supported() const noexcept override {return false;}
    [[nodiscard]] bool can_undo() const noexcept override {return false;}
    [[nodiscard]] bool can_redo() const noexcept override {return false;}
    [[nodiscard]] bool reset_supported() const noexcept override {return false;}
    [[nodiscard]] QString editor_content_key() const override
    {return dynamic()?QStringLiteral("DYNAMIC_EQ"):QStringLiteral("TRANSIENT_SHAPER");}
    [[nodiscard]] QString live_change_policy() const override
    {return QStringLiteral("PRODUCT_REF_LINKED_READ_ONLY");}
    [[nodiscard]] GainViewModel* gain_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] EqViewModel* eq_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] CompressorViewModel* compressor_view_model() const noexcept override {return nullptr;}
    [[nodiscard]] QString family_accent() const override
    {return dynamic()?QStringLiteral("#4E86C6"):QStringLiteral("#C4774A");}
    [[nodiscard]] QString state_text() const override
    {
        const auto s=current();
        if(!s)return QStringLiteral("Unavailable");
        return s->instance.active()?QStringLiteral("REF LINKED · Read only"):
            QStringLiteral("REF LINKED · Inactive");
    }
    [[nodiscard]] bool has_error() const noexcept override {return !current();}
    [[nodiscard]] bool linked_read_only() const noexcept override {return true;}
    [[nodiscard]] QVariantMap linked_parameters() const override
    {const auto s=current();return s?project_linked_editor_parameters(*s):QVariantMap{};}

    [[nodiscard]] QVariantList linkedBandResponse(int index) const override
    {const auto s=current();return s?project_linked_dynamic_response(*s,index):QVariantList{};}

    // Neither QML nor a caller holding the old adapter can mutate the
    // linked chain, bypass it, trigger a render or manufacture history.
    void setBypass(bool) override {}
    void undo() override {}
    void redo() override {}
    void resetToDefault() override {}

private:
    [[nodiscard]] bool dynamic() const noexcept {return type_=="rgsml.dsp.dynamic-eq";}
    [[nodiscard]] std::optional<LinkedEditorSnapshot> current() const
    {
        if(!preview_||!manual_)return {};
        const auto owner=exact_owner_.lock();
        if(!owner)return {};
        const auto snapshots=inspect_current_linked_editors(*preview_,*manual_);
        if(!snapshots)return {};
        const auto it=std::find_if(snapshots->begin(),snapshots->end(),
            [&](const auto& s){return s.exact_owner==owner&&
                s.instance.instance_id()==id_&&s.instance.module_type_id()==type_;});
        if(it==snapshots->end())return {};
        return *it;
    }
    QPointer<MasteringPreviewController> preview_;
    QPointer<AuditionSourceSelector> selector_;
    QPointer<PlaybackTransportViewModel> transport_;
    MasteringChainState* manual_{};
    std::weak_ptr<const LinkedEditorSnapshot::Ticket> exact_owner_;
    dsp::ModuleInstanceId id_;
    std::string type_;
    QString context_;
};
} // namespace rgsml::app::l3_automatch
