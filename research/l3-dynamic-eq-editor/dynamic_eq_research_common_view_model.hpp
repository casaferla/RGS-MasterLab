#pragma once
// Research-only Parametric-grammar reader of ONE accepted C01 band store.
// Does not edit Product, calculate a detector or invent an Overall response.
#include "dynamic_eq_research_dynamics_model.hpp"
#include "../../dsp/src/internal/dynamic_eq_c01_coefficients.hpp"
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <variant>

namespace rgsml::research::dynamic_ui {
class DynamicEqResearchCommonViewModel final:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool manualEditable READ manualEditable CONSTANT)
    Q_PROPERTY(int bandCount READ bandCount NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY changed)
    Q_PROPERTY(QString selectedBandId READ selectedBandId NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(QString filter READ filter NOTIFY changed)
    Q_PROPERTY(QString routing READ routing NOTIFY changed)
    Q_PROPERTY(double frequency READ frequency NOTIFY changed)
    Q_PROPERTY(double maximumFrequencyHz READ maximumFrequencyHz NOTIFY changed)
    Q_PROPERTY(double gain READ gain NOTIFY changed)
    Q_PROPERTY(double q READ q NOTIFY changed)
    Q_PROPERTY(double shelfSlope READ shelfSlope NOTIFY changed)
    Q_PROPERTY(QString frequencyText READ frequencyText NOTIFY changed)
    Q_PROPERTY(QString gainText READ gainText NOTIFY changed)
    Q_PROPERTY(QString qText READ qText NOTIFY changed)
    Q_PROPERTY(QString shelfSlopeText READ shelfSlopeText NOTIFY changed)
    Q_PROPERTY(bool gainApplicable READ gainApplicable NOTIFY changed)
    Q_PROPERTY(bool qApplicable READ qApplicable NOTIFY changed)
    Q_PROPERTY(bool shelfSlopeApplicable READ shelfSlopeApplicable NOTIFY changed)
    Q_PROPERTY(bool slopeApplicable READ slopeApplicable CONSTANT)
    Q_PROPERTY(bool addAvailable READ addAvailable CONSTANT)
    Q_PROPERTY(bool removeAvailable READ removeAvailable CONSTANT)
    Q_PROPERTY(bool routeAvailable READ routeAvailable NOTIFY changed)
    Q_PROPERTY(bool mixedRouting READ mixedRouting NOTIFY changed)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)
    Q_PROPERTY(bool bypass READ bypass CONSTANT)
    Q_PROPERTY(QVariantList bandSummaries READ bandSummaries NOTIFY changed)
    Q_PROPERTY(QVariantList selectedBandResponsePoints READ selectedBandResponsePoints NOTIFY changed)
    Q_PROPERTY(QString validationField READ validationField CONSTANT)
    Q_PROPERTY(QString validationMessage READ validationMessage CONSTANT)
    Q_PROPERTY(QString previewStatus READ previewStatus CONSTANT)
    Q_PROPERTY(QString previewError READ previewError CONSTANT)
    Q_PROPERTY(QString responseSemantics READ responseSemantics CONSTANT)
public:
    explicit DynamicEqResearchCommonViewModel(
        DynamicEqResearchDynamicsModel& source,QObject* parent=nullptr)
        :QObject(parent),source_(&source) {
        connect(source_,&DynamicEqResearchDynamicsModel::changed,
                this,&DynamicEqResearchCommonViewModel::changed);
    }
    [[nodiscard]] bool manualEditable() const noexcept {return false;}
    [[nodiscard]] int bandCount() const noexcept {return int(bands().size());}
    [[nodiscard]] int selectedIndex() const noexcept {
        return int(source_->selectedBandIndex());
    }
    [[nodiscard]] QString selectedBandId() const {
        return loaded()?QString::fromStdString(spec().band_id.to_string()):QString{};
    }
    [[nodiscard]] bool enabled() const noexcept {return loaded()&&spec().enabled;}
    [[nodiscard]] QString filter() const {
        return loaded()?filterName(spec().filter_type):"BELL";
    }
    [[nodiscard]] QString routing() const {
        return loaded()?routeName(spec().routing):"STEREO";
    }
    [[nodiscard]] double frequency() const noexcept {
        return loaded()?std::visit([](const auto& b){return b.frequency_hz;},
                                   spec().shape):1000.0;
    }
    [[nodiscard]] double maximumFrequencyHz() const noexcept {
        return std::min(20000.0,0.45*double(fs_));
    }
    [[nodiscard]] double gain() const noexcept {
        return loaded()?spec().control.base_gain_db:0.0;
    }
    [[nodiscard]] double q() const noexcept {
        if(!loaded())return 0.707;
        const auto* b=std::get_if<rgsml::dsp::DynamicEqBellShape>(&spec().shape);
        return b?b->q:0.707;
    }
    [[nodiscard]] double shelfSlope() const noexcept {
        if(!loaded())return 1.0;
        const auto* s=std::get_if<rgsml::dsp::DynamicEqShelfShape>(&spec().shape);
        return s?s->shelf_slope:1.0;
    }
    [[nodiscard]] QString frequencyText() const {return num(frequency());}
    [[nodiscard]] QString gainText() const {return num(gain());}
    [[nodiscard]] QString qText() const {return num(q());}
    [[nodiscard]] QString shelfSlopeText() const {return num(shelfSlope());}
    [[nodiscard]] bool gainApplicable() const noexcept {return loaded();}
    [[nodiscard]] bool qApplicable() const noexcept {
        return !loaded()||spec().filter_type==rgsml::dsp::DynamicEqFilterType::BELL;
    }
    [[nodiscard]] bool shelfSlopeApplicable() const noexcept {
        return loaded()&&!qApplicable();
    }
    [[nodiscard]] bool slopeApplicable() const noexcept {return false;}
    [[nodiscard]] bool addAvailable() const noexcept {return false;}
    [[nodiscard]] bool removeAvailable() const noexcept {return false;}
    [[nodiscard]] bool routeAvailable() const noexcept {return channels_==2;}
    [[nodiscard]] bool mixedRouting() const noexcept {
        if(bands().size()<2)return false;
        const auto first=bands().front().routing;
        return std::any_of(bands().cbegin()+1,bands().cend(),
                           [&](const auto& b){return b.routing!=first;});
    }
    [[nodiscard]] bool isDefault() const noexcept {
        return bands().size()==1&&spec().control.base_gain_db==0.0
               &&spec().control.range_db==0.0;
    }
    [[nodiscard]] bool bypass() const noexcept {return false;}
    [[nodiscard]] QString validationField() const {return {};}
    [[nodiscard]] QString validationMessage() const {return {};}
    [[nodiscard]] QString previewStatus() const {
        return QStringLiteral("RESEARCH_READ_ONLY");
    }
    [[nodiscard]] QString previewError() const {return {};}
    [[nodiscard]] QString responseSemantics() const {
        return QStringLiteral("C01 BASE GAIN RESPONSE, NOT APPLIED ACTIVITY");
    }
    [[nodiscard]] QVariantList bandSummaries() const {
        QVariantList out;
        for(std::size_t i=0;i<bands().size();++i) {
            const auto& b=bands()[i];
            const auto f=std::visit(
                [](const auto& s){return s.frequency_hz;},b.shape);
            out.append(QVariantMap{
                {"index",int(i)},
                {"bandId",QString::fromStdString(b.band_id.to_string())},
                {"enabled",b.enabled},
                {"filter",filterName(b.filter_type)},
                {"routing",routeName(b.routing)},
                {"frequency",f},
                {"gain",b.control.base_gain_db},
                {"gainApplicable",true}});
        }
        return out;
    }
    [[nodiscard]] QVariantList selectedBandResponsePoints() const {
        // Exact accepted C01 coefficients evaluated at static base_gain_db.
        // Real per-frame applied gain is exclusively the Activity sidecar.
        QVariantList out;
        if(!loaded()||!spec().enabled)return out;
        auto b=rgsml::dsp::DynamicEqBandParameters::create(spec());
        if(!b)return out;
        auto generator=rgsml::dsp::internal::C01CoefficientGenerator::create(
            *b.value(),fs_,channels_);
        if(!generator)return out;
        auto coeff=generator.value()->audio_coefficients(gain());
        if(!coeff)return out;
        const auto c=*coeff.value();
        constexpr int count=257;
        out.reserve(count);
        for(int i=0;i<count;++i) {
            const auto freq=20.0*std::pow(maximumFrequencyHz()/20.0,
                                         double(i)/(count-1));
            const auto z=std::polar(1.0,-2.0*std::numbers::pi*freq/double(fs_));
            const std::complex<double> top=c.b0+c.b1*z+c.b2*z*z;
            const std::complex<double> bottom=1.0+c.a1*z+c.a2*z*z;
            if(std::abs(bottom)==0.0)return {};
            const auto h=top/bottom;
            const auto mag=std::abs(h);
            if(!std::isfinite(mag)||mag<=0.0)return {};
            const auto db=20.0*std::log10(mag);
            if(!std::isfinite(db))return {};
            out.append(QVariantMap{{"frequency",freq},{"magnitudeDb",db},
                                   {"phaseRad",std::arg(h)}});
        }
        return out;
    }
    [[nodiscard]] bool setResearchAudioFormat(
        std::uint32_t fs,std::uint32_t channels) {
        if(fs==0||channels<1||channels>2)return false;
        for(const auto& spec:bands()) {
            auto b=rgsml::dsp::DynamicEqBandParameters::create(spec);
            if(!b || !b.value()->validate_for_audio(fs,channels))return false;
        }
        fs_=fs;channels_=channels;emit changed();return true;
    }
    Q_INVOKABLE void selectBand(int index) {
        if(index>=0&&std::size_t(index)<bands().size())
            static_cast<void>(source_->selectBand(std::size_t(index)));
    }
signals:
    void changed();
private:
    [[nodiscard]] bool loaded() const noexcept {return !bands().empty();}
    [[nodiscard]] const std::vector<rgsml::dsp::DynamicEqBandSpec>&
    bands() const noexcept {return source_->acceptedBandSpecs();}
    [[nodiscard]] const rgsml::dsp::DynamicEqBandSpec& spec() const {
        return bands()[source_->selectedBandIndex()];
    }
    [[nodiscard]] static QString num(double v) {
        return QString::number(v,'g',8);
    }
    [[nodiscard]] static QString filterName(rgsml::dsp::DynamicEqFilterType f) {
        using rgsml::dsp::DynamicEqFilterType;
        switch(f) {
        case DynamicEqFilterType::BELL:return "BELL";
        case DynamicEqFilterType::LOW_SHELF:return "LOW_SHELF";
        case DynamicEqFilterType::HIGH_SHELF:return "HIGH_SHELF";
        }
        return "UNKNOWN";
    }
    [[nodiscard]] static QString routeName(rgsml::dsp::EqRouting r) {
        using rgsml::dsp::EqRouting;
        switch(r) {
        case EqRouting::STEREO:return "STEREO";
        case EqRouting::LEFT:return "LEFT";
        case EqRouting::RIGHT:return "RIGHT";
        case EqRouting::MID:return "MID";
        case EqRouting::SIDE:return "SIDE";
        }
        return "UNKNOWN";
    }
    DynamicEqResearchDynamicsModel* source_;
    std::uint32_t fs_{48000U},channels_{2U};
};
} // namespace rgsml::research::dynamic_ui
