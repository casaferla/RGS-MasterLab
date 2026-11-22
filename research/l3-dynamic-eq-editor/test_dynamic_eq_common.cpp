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
    // No edits in L3 unless a TEST explicitly opts into research authoring.
    common.setFilter("BELL");
    if(common.filter()!="LOW_SHELF")return 14;
    owner.setResearchManualEditable(true);
    if(!common.manualEditable())return 15;
    common.setDraftFieldText("gain","15"); // 15+range4 exceeds +18.
    if(common.gainText()!="15")return 26;
    if(common.commitDraft()||common.gain()!=-3.0)return 16;
    common.cancelDraft();
    if(common.gainText()=="15")return 27;
    common.setDraftFieldText("gain","10");
    if(!common.commitDraft()||common.gain()!=10.0
       ||owner.acceptedBandSpecs()[1].control.base_gain_db!=10.0)
        return 17;
    common.setFilter("NOTCH"); // not part of frozen C01
    if(common.filter()!="LOW_SHELF")return 18;
    common.setFilter("BELL");
    if(common.filter()!="BELL"||!common.qApplicable())return 19;
    common.setDraftFieldValue("q",1.6);
    if(!common.commitDraft()||common.q()!=1.6)return 20;
    common.graphDrag(1600.0,22.0); // C01 signed Range limits Base.
    common.graphRelease();
    if(common.frequency()!=1600.0||common.gain()!=14.0)return 21;
    if(!common.setResearchAudioFormat(44100U,2U))return 22;
    common.setRouting("SIDE");
    if(common.routing()!="SIDE")return 23;
    common.setRouting("NOT_A_ROUTE");
    if(common.routing()!="SIDE")return 24;
    auto id3=core::Uuid::parse("33333333-3333-4333-8333-333333333333");
    if(!id3)return 28;
    owner.setResearchIdGenerator([id=*id3.value()](){return id;});
    if(!common.addAvailable()||common.removeAvailable()==false)return 29;
    // With two bands, removal is valid; addition uses injected unique UUID.
    common.addBand();
    if(common.bandCount()!=3||common.selectedIndex()!=2
       ||common.selectedBandId()!=QString::fromStdString(id3.value()->to_string()))
        return 30;
    common.undo();
    if(common.bandCount()!=2||!common.canRedo())return 31;
    common.redo();
    if(common.bandCount()!=3)return 32;
    common.removeSelectedBand();
    if(common.bandCount()!=2||common.selectedIndex()!=1)return 33;
    common.undo();
    if(common.bandCount()!=3)return 34;
    owner.setResearchManualEditable(false);
    if(common.addAvailable()||common.removeAvailable()||common.canUndo())
        return 35;
    common.setFilter("LOW_SHELF");
    if(common.filter()!="BELL"||common.manualEditable())return 25;
    std::cout<<"Dynamic EQ typed research common + Dynamics single state: PASS\n";
    return 0;
}
