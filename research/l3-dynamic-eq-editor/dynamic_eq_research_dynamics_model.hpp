#pragma once
// Isolated, opt-in C01 parameter presentation. Never a second DSP.
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <cmath>
#include <algorithm>
#include <functional>
#include <optional>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace rgsml::research::dynamic_ui {
class DynamicEqResearchDynamicsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
    Q_PROPERTY(bool manualEditable READ manualEditable NOTIFY changed)
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QVariantMap texts READ texts NOTIFY changed)
    Q_PROPERTY(QString validationField READ validationField NOTIFY changed)
    Q_PROPERTY(QString validationMessage READ validationMessage NOTIFY changed)
public:
    explicit DynamicEqResearchDynamicsModel(QObject* parent=nullptr):QObject(parent){}
    [[nodiscard]] bool loaded() const noexcept {return !bands_.empty();}
    // The common EQ presenter observes these accepted specs by reference:
    // no duplicate state or parameter authority is permitted.
    [[nodiscard]] const std::vector<rgsml::dsp::DynamicEqBandSpec>&
    acceptedBandSpecs() const noexcept {return bands_;}
    [[nodiscard]] std::size_t selectedBandIndex() const noexcept {
        return selected_;
    }
    [[nodiscard]] bool manualEditable() const noexcept {return editable_;}
    [[nodiscard]] QVariantMap values() const {
        const auto& c=control();
        return {{"range",c.range_db},{"threshold",c.threshold_dbfs},
                {"ratio",c.ratio},{"rmsTime",c.rms_time_constant_ms},
                {"attack",c.attack_ms},{"release",c.release_ms}};
    }
    [[nodiscard]] QVariantMap texts() const {
        QVariantMap t;
        const auto v=values();
        for(auto it=v.cbegin();it!=v.cend();++it)
            t.insert(it.key(), pending_key_==it.key()?pending_text_
                :QString::number(it.value().toDouble(),'g',8));
        return t;
    }
    [[nodiscard]] QString validationField() const {return error_key_;}
    [[nodiscard]] QString validationMessage() const {return error_text_;}
    [[nodiscard]] QString pendingFieldTextOr(const QString& key,double fallback) const {
        return pending_key_==key ? pending_text_
            :QString::number(fallback,'g',8);
    }
    using IdGenerator=std::function<rgsml::core::Uuid()>;
    void setResearchIdGenerator(IdGenerator generator) {
        id_generator_=std::move(generator);emit changed();
    }
    [[nodiscard]] bool canAddBand() const noexcept {
        return editable_ && loaded() && bands_.size()<6U
            && bool(id_generator_);
    }
    [[nodiscard]] bool canRemoveBand() const noexcept {
        return editable_ && bands_.size()>1U;
    }
    [[nodiscard]] bool canUndo() const noexcept {
        return editable_ && !undo_.empty();
    }
    [[nodiscard]] bool canRedo() const noexcept {
        return editable_ && !redo_.empty();
    }
    [[nodiscard]] bool addResearchBand() {
        if(!canAddBand())return false;
        const auto uid=id_generator_();
        if(uid.is_nil())return false;
        for(const auto& b:bands_)if(b.band_id==uid)return false;
        rgsml::dsp::DynamicEqBandSpec next;
        next.band_id=uid;
        auto typed=rgsml::dsp::DynamicEqBandParameters::create(next);
        if(!typed||!typed.value()->validate_for_audio(fs_,channels_))
            return false;
        pushUndo();bands_.push_back(next);
        selected_=bands_.size()-1;cancelDraft();emit changed();return true;
    }
    [[nodiscard]] bool removeResearchBand() {
        if(!canRemoveBand())return false;
        pushUndo();
        bands_.erase(bands_.begin()+static_cast<std::ptrdiff_t>(selected_));
        selected_=std::min(selected_,bands_.size()-1);
        cancelDraft();emit changed();return true;
    }
    void undoResearchEdit() {
        if(!canUndo())return;
        redo_.push_back(snapshot());
        auto target=std::move(undo_.back());undo_.pop_back();
        restore(std::move(target));
    }
    void redoResearchEdit() {
        if(!canRedo())return;
        undo_.push_back(snapshot());
        auto target=std::move(redo_.back());redo_.pop_back();
        restore(std::move(target));
    }
    [[nodiscard]] bool loadAcceptedParameters(
        const rgsml::dsp::DynamicEqParameters& accepted) {
        if(accepted.bands().empty()||accepted.bands().size()>6U)return false;
        bands_.clear();
        for(const auto& b:accepted.bands())bands_.push_back(b.spec());
        selected_=0;
        undo_.clear();redo_.clear();
        cancelDraft();
        emit changed();
        return true;
    }
    [[nodiscard]] bool selectBand(std::size_t n) {
        if(n>=bands_.size())return false;
        // Clicking the already-selected band must not discard an unfinished
        // numeric edit or an in-progress graph gesture.
        if(n==selected_)return true;
        selected_=n; cancelDraft(); graph_draft_.reset();
        emit changed(); return true;
    }
    [[nodiscard]] bool setResearchAudioFormat(std::uint32_t fs,
                                               std::uint32_t channels) {
        if(fs==0||channels<1||channels>2)return false;
        for(const auto& spec:bands_) {
            auto p=rgsml::dsp::DynamicEqBandParameters::create(spec);
            if(!p||!p.value()->validate_for_audio(fs,channels))return false;
        }
        fs_=fs;channels_=channels;cancelDraft();emit changed();
        return true;
    }
    [[nodiscard]] const rgsml::dsp::DynamicEqBandSpec&
    presentedBandSpec() const {
        return graph_draft_ ? *graph_draft_ : bands_[selected_];
    }
    [[nodiscard]] bool setSelectedEnabled(bool value) {
        if(!editable_||!loaded())return false;
        auto next=bands_[selected_];next.enabled=value;
        return commitCommonCandidate(std::move(next));
    }
    [[nodiscard]] bool setSelectedFilter(const QString& token) {
        if(!editable_||!loaded())return false;
        using rgsml::dsp::DynamicEqFilterType;
        using rgsml::dsp::DynamicEqShape;
        std::optional<DynamicEqFilterType> type;
        if(token=="BELL")type=DynamicEqFilterType::BELL;
        if(token=="LOW_SHELF")type=DynamicEqFilterType::LOW_SHELF;
        if(token=="HIGH_SHELF")type=DynamicEqFilterType::HIGH_SHELF;
        if(!type)return false;
        auto next=bands_[selected_];
        // Reselecting the active filter is a true no-op. Reconstructing
        // its shape would silently erase user-authored Q or Shelf Slope.
        if(next.filter_type==*type)return true;
        const auto freq=std::visit(
            [](const auto& s){return s.frequency_hz;},next.shape);
        next.filter_type=*type;
        if(*type==DynamicEqFilterType::BELL)
            next.shape=rgsml::dsp::DynamicEqBellShape{freq,0.707};
        else
            next.shape=rgsml::dsp::DynamicEqShelfShape{freq,1.0};
        return commitCommonCandidate(std::move(next));
    }
    [[nodiscard]] bool setSelectedRouting(const QString& token) {
        if(!editable_||!loaded())return false;
        using rgsml::dsp::EqRouting;
        std::optional<EqRouting> route;
        if(token=="STEREO")route=EqRouting::STEREO;
        if(token=="LEFT")route=EqRouting::LEFT;
        if(token=="RIGHT")route=EqRouting::RIGHT;
        if(token=="MID")route=EqRouting::MID;
        if(token=="SIDE")route=EqRouting::SIDE;
        if(!route || (channels_==1&&*route!=EqRouting::STEREO))
            return false;
        auto next=bands_[selected_];next.routing=*route;
        return commitCommonCandidate(std::move(next));
    }
    [[nodiscard]] bool resetResearchToFlat() {
        if(!editable_||!loaded())return false;
        rgsml::dsp::DynamicEqBandSpec flat;
        flat.band_id=bands_.front().band_id;
        auto checked=rgsml::dsp::DynamicEqBandParameters::create(flat);
        if(!checked)return false;
        pushUndo();bands_.assign(1,flat);selected_=0;
        graph_draft_.reset();cancelDraft();emit changed();return true;
    }
    void setGraphDraft(double frequency,double gain) {
        if(!editable_||!loaded()||!std::isfinite(frequency)
           ||!std::isfinite(gain))return;
        auto next=bands_[selected_];
        const auto range=next.control.range_db;
        const double min_gain=std::max(-18.0,-18.0-range);
        const double max_gain=std::min(18.0,18.0-range);
        next.control.base_gain_db=std::clamp(gain,min_gain,max_gain);
        const double f=std::clamp(
            frequency,20.0,std::min(20000.0,0.45*double(fs_)));
        std::visit([&](auto& s){s.frequency_hz=f;},next.shape);
        graph_draft_=std::move(next);emit changed();
    }
    [[nodiscard]] bool commitGraphDraft() {
        if(!graph_draft_)return false;
        auto next=*graph_draft_;
        graph_draft_.reset();
        return commitCommonCandidate(std::move(next));
    }
    void setResearchManualEditable(bool enabled) {
        if(editable_==enabled)return;
        // A real permission transition revokes any uncommitted text/drag.
        // Repeated identical permission notifications must not lose a draft.
        editable_=enabled; cancelDraft(); emit changed();
    }
    Q_INVOKABLE void setDraftFieldText(QString key,QString raw) {
        if(!loaded()||!editable_||
            !(values().contains(key)||key=="frequency"||key=="gain"
              ||key=="q"||key=="shelfSlope"))return;
        graph_draft_.reset();
        pending_key_=std::move(key);pending_text_=std::move(raw);
        error_key_.clear();error_text_.clear();emit changed();
    }
    Q_INVOKABLE void setDraftFieldValue(QString key,double number) {
        if(std::isfinite(number))
            setDraftFieldText(std::move(key),QString::number(number,'g',12));
    }
    Q_INVOKABLE bool commitDraft() {
        if(!loaded()||!editable_||pending_key_.isEmpty())return false;
        bool valid=false;
        const double n=pending_text_.toDouble(&valid);
        if(!valid||!std::isfinite(n))return fail("INVALID_NUMBER");
        auto next=bands_[selected_];
        auto& c=next.control;
        if(pending_key_=="range")c.range_db=n;
        else if(pending_key_=="threshold")c.threshold_dbfs=n;
        else if(pending_key_=="ratio")c.ratio=n;
        else if(pending_key_=="rmsTime")c.rms_time_constant_ms=n;
        else if(pending_key_=="attack")c.attack_ms=n;
        else if(pending_key_=="release")c.release_ms=n;
        else if(pending_key_=="gain")c.base_gain_db=n;
        else if(pending_key_=="frequency")
            std::visit([&](auto& s){s.frequency_hz=n;},next.shape);
        else if(pending_key_=="q") {
            auto* shape=std::get_if<rgsml::dsp::DynamicEqBellShape>(&next.shape);
            if(!shape)return fail("C01_Q_NOT_APPLICABLE");
            shape->q=n;
        } else if(pending_key_=="shelfSlope") {
            auto* shape=std::get_if<rgsml::dsp::DynamicEqShelfShape>(&next.shape);
            if(!shape)return fail("C01_SHELF_SLOPE_NOT_APPLICABLE");
            shape->shelf_slope=n;
        } else return false;
        return commitCommonCandidate(std::move(next));
    }
    Q_INVOKABLE void cancelDraft() {
        pending_key_.clear();pending_text_.clear();
        error_key_.clear();error_text_.clear();graph_draft_.reset();
        emit changed();
    }
    [[nodiscard]] bool validatedSnapshot() const {
        if(!loaded())return false;
        std::vector<rgsml::dsp::DynamicEqBandParameters> checked;
        for(const auto& spec:bands_) {
            auto b=rgsml::dsp::DynamicEqBandParameters::create(spec);
            if(!b)return false;
            checked.push_back(std::move(*b.value()));
        }
        return bool(rgsml::dsp::DynamicEqParameters::create(std::move(checked)));
    }
private:
    [[nodiscard]] bool commitCommonCandidate(
        rgsml::dsp::DynamicEqBandSpec next) {
        if(!editable_||!loaded())return false;
        auto checked=rgsml::dsp::DynamicEqBandParameters::create(next);
        if(!checked || !checked.value()->validate_for_audio(fs_,channels_))
            return fail("C01_OUT_OF_RANGE_OR_ENDPOINT");
        if(bands_[selected_]!=next)pushUndo();
        bands_[selected_]=std::move(next);cancelDraft();emit changed();
        return true;
    }
    struct Snapshot {
        std::vector<rgsml::dsp::DynamicEqBandSpec> bands;
        std::size_t selected{0};
    };
    [[nodiscard]] Snapshot snapshot() const {
        return Snapshot{bands_,selected_};
    }
    void pushUndo() {
        undo_.push_back(snapshot());
        if(undo_.size()>50U)undo_.erase(undo_.begin());
        redo_.clear();
    }
    void restore(Snapshot previous) {
        bands_=std::move(previous.bands);
        selected_=previous.selected;
        cancelDraft();emit changed();
    }
    [[nodiscard]] bool fail(const char* error) {
        error_key_=pending_key_;error_text_=QString::fromLatin1(error);
        emit changed();return false;
    }
    [[nodiscard]] const rgsml::dsp::DynamicEqControl& control() const {
        if(loaded())return bands_[selected_].control;
        static const rgsml::dsp::DynamicEqControl defaults{};
        return defaults;
    }
signals:
    void changed();
private:
    std::vector<rgsml::dsp::DynamicEqBandSpec> bands_;
    std::size_t selected_{0};
    bool editable_{false};
    std::uint32_t fs_{48000U},channels_{2U};
    std::optional<rgsml::dsp::DynamicEqBandSpec> graph_draft_;
    IdGenerator id_generator_;
    std::vector<Snapshot> undo_,redo_;
    QString pending_key_,pending_text_,error_key_,error_text_;
};
}
