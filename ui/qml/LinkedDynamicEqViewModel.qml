import QtQuick

// No second parameter store: each field reads the current native adapter.
// Local state is only the selected band's presentation index.
QtObject {
    id: root
    property var module: null
    readonly property var parameters: module ? module.linkedParameters : ({})
    readonly property var bandSummaries: parameters.bands || []
    readonly property int bandCount: bandSummaries.length
    property int selectedIndex: 0
    readonly property var band: bandCount > 0
        ? bandSummaries[Math.min(selectedIndex, bandCount-1)] : ({})
    readonly property bool loaded: bandCount > 0
    readonly property bool manualEditable: false
    readonly property bool enabled: loaded && band.enabled
    readonly property string selectedBandId: band.bandId || ""
    readonly property string filter: band.filter || ""
    readonly property string routing: band.routing || ""
    readonly property real frequency: band.frequency === undefined ? 0 : band.frequency
    readonly property real maximumFrequencyHz: parameters.maximumFrequencyHz || 20000
    readonly property real gain: band.gain === undefined ? 0 : band.gain
    readonly property real q: band.q === undefined ? 0 : band.q
    readonly property real shelfSlope: band.shelfSlope === undefined ? 0 : band.shelfSlope
    readonly property string frequencyText: loaded ? String(frequency) : "—"
    readonly property string gainText: loaded ? String(gain) : "—"
    readonly property string qText: qApplicable ? String(q) : "—"
    readonly property string shelfSlopeText: shelfSlopeApplicable ? String(shelfSlope) : "—"
    readonly property bool gainApplicable: loaded && band.gainApplicable
    readonly property bool qApplicable: loaded && band.qApplicable
    readonly property bool shelfSlopeApplicable: loaded && band.shelfSlopeApplicable
    readonly property bool slopeApplicable: false
    readonly property int slopeDbPerOct: 0
    readonly property bool addAvailable: false
    readonly property bool removeAvailable: false
    readonly property bool canUndo: false
    readonly property bool canRedo: false
    readonly property bool routeAvailable: loaded
    readonly property bool mixedRouting: {
        for (let i=1;i<bandCount;++i)
            if (bandSummaries[i].routing !== bandSummaries[0].routing) return true
        return false
    }
    readonly property bool bypass: !module || module.bypass
    readonly property string validationField: ""
    readonly property string validationMessage: ""
    readonly property string previewStatus: module ? module.previewStatus : "UNAVAILABLE"
    readonly property string previewError: module ? module.previewError : ""
    readonly property string responseSemantics: "C01 BASE GAIN RESPONSE, NOT APPLIED ACTIVITY"
    readonly property var selectedBandResponsePoints: loaded && module
        ? module.linkedBandResponse(selectedIndex) : []
    readonly property var values: band.dynamics || ({})
    readonly property var texts: {
        let t={}
        for (let key in values) t[key]=String(values[key])
        return t
    }
    signal changed()
    onSelectedBandResponsePointsChanged: changed()
    onBandSummariesChanged: {
        if (selectedIndex >= bandCount) selectedIndex=0
        changed()
    }
    onModuleChanged: selectedIndex=0
    function selectBand(index) {
        if(index>=0 && index<bandCount) selectedIndex=index
    }
    // Defensive no-op editing interface used by the shared research controls.
    function setDraftFieldText(field, text) {}
    function setDraftFieldValue(field, value) {}
    function commitDraft() { return false }
    function cancelDraft() {}
    function setEnabled(value) {}
    function setFilter(value) {}
    function setRouting(value) {}
    function resetToFlat() {}
    function addBand() {}
    function removeSelectedBand() {}
    function undo() {}
    function redo() {}
    function graphDrag(frequency, gain) {}
    function graphRelease() {}
    function adjustSecondaryParameter(steps, fine) {}
    function setDraftSlopeDbPerOct(value) {}
}
