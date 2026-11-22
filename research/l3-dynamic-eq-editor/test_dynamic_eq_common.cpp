#include "dynamic_eq_research_common_view_model.hpp"
#include <rgsml/core/uuid.hpp>
#include <QCoreApplication>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
using namespace rgsml;
using namespace rgsml::research::dynamic_ui;
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    DynamicEqResearchDynamicsModel owner;
    DynamicEqResearchCommonViewModel common(owner);
    if(common.bandCount()!=0||common.manualEditable())return 2;
    auto id1=core::Uuid::parse("11111111-1111-4111-8111-111111111111");
    auto id2=core::Uuid::parse("22222222-2222-4222-8222-222222222222");
    if(!id1||!id2)return 3;
    dsp::DynamicEqBandSpec b1;
    b1.band_id=*id1.value();
    b1.control.base_gain_db=4.0;
    b1.control.range_db=-5.0;
    dsp::DynamicEqBandSpec b2;
    b2.band_id=*id2.value();
    b2.filter_type=dsp::DynamicEqFilterType::LOW_SHELF;
    b2.shape=dsp::DynamicEqShelfShape{880.0,0.8};
    b2.control.base_gain_db=-3.0;
    b2.control.range_db=4.0;
    auto first=dsp::DynamicEqBandParameters::create(b1);
    auto second=dsp::DynamicEqBandParameters::create(b2);
    if(!first||!second)return 4;
    auto p=dsp::DynamicEqParameters::create({*first.value(),*second.value()});
    if(!p||!owner.loadAcceptedParameters(*p.value()))return 5;
    if(common.bandCount()!=2||common.filter()!="BELL"
       ||common.gain()!=4||common.selectedBandResponsePoints().size()!=257
       ||!common.mixedRouting()==false)return 6;
    common.selectBand(1);
    if(owner.selectedBandIndex()!=1||common.selectedIndex()!=1
       ||common.filter()!="LOW_SHELF"||common.frequency()!=880.0
       ||common.gain()!=-3.0||common.shelfSlope()!=0.8
       ||common.qApplicable()||!common.shelfSlopeApplicable())return 7;
    if(common.selectedBandId()!=QString::fromStdString(b2.band_id.to_string()))
       return 8;
    if(common.selectedBandResponsePoints().size()!=257)return 9;
    if(common.setResearchAudioFormat(44100U,2U)==false
       ||common.maximumFrequencyHz()!=19845.0)return 10;
    if(common.setResearchAudioFormat(44100U,1U)==false)return 11;
    if(common.manualEditable()||common.addAvailable()
       ||common.removeAvailable())return 12;
    if(owner.values().value("range").toDouble()!=4.0)return 13;
    std::cout<<"Dynamic EQ read-only common C01 view shares selected band: PASS\n";
    return 0;
}
