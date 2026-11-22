#pragma once
// Isolated, opt-in C01 parameter presentation. Never a second DSP.
#include <rgsml/dsp/dynamic_eq_parameters.hpp>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <cmath>
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
    [[nodiscard]] bool loadAcceptedParameters(
        const rgsml::dsp::DynamicEqParameters& accepted) {
        if(accepted.bands().empty()||accepted.bands().size()>6U)return false;
        bands_.clear();
        for(const auto& b:accepted.bands())bands_.push_back(b.spec());
        selected_=0;
        cancelDraft();
        emit changed();
        return true;
    }
    [[nodiscard]] bool selectBand(std::size_t n) {
        if(n>=bands_.size())return false;
        selected_=n; cancelDraft(); emit changed(); return true;
    }
    void setResearchManualEditable(bool enabled) {
        editable_=enabled; cancelDraft(); emit changed();
    }
    Q_INVOKABLE void setDraftFieldText(QString key,QString raw) {
        if(!loaded()||!editable_||!values().contains(key))return;
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
        else return false;
        if(!rgsml::dsp::DynamicEqBandParameters::create(next))
            return fail("C01_OUT_OF_RANGE_OR_ENDPOINT");
        bands_[selected_]=std::move(next);
        cancelDraft();emit changed();return true;
    }
    Q_INVOKABLE void cancelDraft() {
        pending_key_.clear();pending_text_.clear();
        error_key_.clear();error_text_.clear();emit changed();
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
    QString pending_key_,pending_text_,error_key_,error_text_;
};
}
