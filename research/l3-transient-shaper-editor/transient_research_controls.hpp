#pragma once
// Research-only QObject: defaults to manual edits locked.
#include <rgsml/dsp/transient_shaper_parameters.hpp>
#include <QObject>

namespace rgsml::research::transient_ui {
class TransientResearchControls final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double attackAmountDb READ attackAmountDb NOTIFY changed)
    Q_PROPERTY(double sustainAmountDb READ sustainAmountDb NOTIFY changed)
    Q_PROPERTY(bool manualEditable READ manualEditable NOTIFY changed)
public:
    explicit TransientResearchControls(QObject* parent=nullptr):QObject(parent){}
    [[nodiscard]] double attackAmountDb() const noexcept {return attack_;}
    [[nodiscard]] double sustainAmountDb() const noexcept {return sustain_;}
    [[nodiscard]] bool manualEditable() const noexcept {return editable_;}
    // Read-only snapshot loading comes from the SAME accepted DSP parameter
    // instance as the PCM whose gain history is presented.
    void loadAcceptedParameters(
        const rgsml::dsp::TransientShaperParameters& p) noexcept {
        const double a=p.attack_amount_db();
        const double s=p.sustain_amount_db();
        if(a==attack_ && s==sustain_)return;
        attack_=a; sustain_=s;
        emit changed();
    }
    // Research-only permission; no production binding.
    void setResearchManualEditable(bool allow) noexcept {
        if(editable_==allow)return;
        editable_=allow;
        emit changed();
    }
    Q_INVOKABLE void setAttackAmountDb(double value) {change(value,sustain_);}
    Q_INVOKABLE void setSustainAmountDb(double value) {change(attack_,value);}
    Q_INVOKABLE void resetToDefault() {change(0.0,0.0);}
signals:
    void changed();
private:
    void change(double attack,double sustain) {
        if(!editable_)return;
        auto p=rgsml::dsp::TransientShaperParameters::create(attack,sustain);
        if(!p || (attack_==attack && sustain_==sustain))return;
        attack_=attack;sustain_=sustain;
        emit changed();
    }
    double attack_{0.0};
    double sustain_{0.0};
    bool editable_{false};
};
}
