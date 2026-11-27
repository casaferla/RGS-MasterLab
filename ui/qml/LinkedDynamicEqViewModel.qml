import QtQuick

// Shared C01 presentation. Only native MANUAL adapters may commit edits.
// Local drafts never become audio parameters until accepted by that owner.
QtObject {
    id: root
    property var module: null
    readonly property var parameters: module ? module.linkedParameters : ({})
    readonly property var committedBands: parameters.bands || []
    property string draftBandId: ""
    property string draftField: ""
    property string draftText: ""
    property var graphDraft: null
    readonly property var bandSummaries: {
        if (!graphDraft) return committedBands
        return committedBands.map(function(b) {
            if (b.bandId !== graphDraft.id) return b
            return Object.assign({}, b, {frequency: graphDraft.hz, gain: graphDraft.gain})
        })
    }
    readonly property int bandCount: bandSummaries.length
    property int selectedIndex: 0
    readonly property var band: bandCount > 0
        ? bandSummaries[Math.min(selectedIndex, bandCount-1)] : ({})
    readonly property bool loaded: bandCount > 0
    readonly property bool manualEditable: !!module && !module.linkedReadOnly
        && module.manualEditable === true
    readonly property bool enabled: loaded && band.enabled
    readonly property string selectedBandId: band.bandId || ""
    readonly property string filter: band.filter || ""
    readonly property string routing: band.routing || ""
    readonly property real frequency: band.frequency === undefined ? 0 : band.frequency
    readonly property real maximumFrequencyHz: parameters.maximumFrequencyHz || 20000
    readonly property real gain: band.gain === undefined ? 0 : band.gain
    readonly property real q: band.q === undefined ? 0 : band.q
    readonly property real shelfSlope: band.shelfSlope === undefined ? 0 : band.shelfSlope
    readonly property string frequencyText: displayText("frequency", loaded ? String(frequency) : "—")
    readonly property string gainText: displayText("gain", loaded ? String(gain) : "—")
    readonly property string qText: displayText("q", qApplicable ? String(q) : "—")
    readonly property string shelfSlopeText: displayText("shelfSlope", shelfSlopeApplicable ? String(shelfSlope) : "—")
    readonly property bool gainApplicable: loaded && band.gainApplicable
    readonly property bool qApplicable: loaded && band.qApplicable
    readonly property bool shelfSlopeApplicable: loaded && band.shelfSlopeApplicable
    readonly property bool slopeApplicable: false
    readonly property int slopeDbPerOct: 0
    readonly property bool addAvailable: manualEditable && bandCount < 6
    readonly property bool removeAvailable: manualEditable && bandCount > 1
    readonly property bool canUndo: manualEditable && module.canUndo
    readonly property bool canRedo: manualEditable && module.canRedo
    readonly property bool routeAvailable: manualEditable ? module.routeAvailable : loaded
    readonly property bool mixedRouting: {
        for (let i=1;i<bandCount;++i)
            if (bandSummaries[i].routing !== bandSummaries[0].routing) return true
        return false
    }
    readonly property bool bypass: !module || module.bypass
    readonly property string validationField: manualEditable ? module.validationField : ""
    readonly property string validationMessage: manualEditable ? module.validationMessage : ""
    readonly property string previewStatus: module ? module.previewStatus : "UNAVAILABLE"
    readonly property string previewError: module ? module.previewError : ""
    readonly property string responseSemantics: "C01 BASE GAIN RESPONSE, NOT APPLIED ACTIVITY"
    readonly property var selectedBandResponsePoints: {
        // Depend on the exact projected snapshot, not only loaded's bool.
        const snapshot = parameters
        return snapshot.instanceId && loaded && module
            ? module.linkedBandResponse(selectedIndex) : []
    }
    readonly property var values: band.dynamics || ({})
    readonly property var texts: {
        let t={}
        for (let key in values) t[key]=displayText(key, String(values[key]))
        return t
    }
    signal changed()
    onSelectedBandResponsePointsChanged: changed()
    onBandSummariesChanged: {
        if (selectedIndex >= bandCount) selectedIndex=0
        changed()
    }
    onModuleChanged: {
        clearLocalDraft()
        selectedIndex=0
    }
    onManualEditableChanged: if (!manualEditable) clearLocalDraft()
    function clearLocalDraft() {
        draftBandId=""
        draftField=""
        draftText=""
        graphDraft=null
    }
    function displayText(field, canonical) {
        return draftBandId === selectedBandId && draftField === field ? draftText : canonical
    }
    function selectBand(index) {
        if (index < 0 || index >= bandCount || index === selectedIndex) return
        if (manualEditable && draftField.length && !commitDraft()) return
        graphDraft=null
        selectedIndex=index
    }
    function setDraftFieldText(field, text) {
        if (!manualEditable || !loaded) return
        if (draftField.length && (draftField !== field || draftBandId !== selectedBandId)
            && !commitDraft()) return
        graphDraft=null
        draftBandId=selectedBandId
        draftField=field
        draftText=String(text)
    }
    function setDraftFieldValue(field, value) { setDraftFieldText(field, String(value)) }
    function commitDraft() {
        if (!manualEditable) return false
        if (graphDraft) return graphRelease()
        if (!draftField.length) return true
        if (!module.editBandText(draftBandId, draftField, draftText)) return false
        clearLocalDraft()
        return true
    }
    function cancelDraft() {
        clearLocalDraft()
        if (manualEditable) module.clearValidation()
    }
    function setEnabled(value) { if (manualEditable && commitDraft()) module.editBandEnabled(selectedBandId,value) }
    function setFilter(value) { if (manualEditable && commitDraft()) module.editBandFilter(selectedBandId,value) }
    function setRouting(value) { if (manualEditable && commitDraft()) module.editBandRouting(selectedBandId,value) }
    function resetToFlat() { if (manualEditable) { cancelDraft(); module.resetToDefault() } }
    function addBand() {
        if (addAvailable && commitDraft() && module.appendBand()) selectedIndex=bandCount-1
    }
    function removeSelectedBand() {
        if (removeAvailable && commitDraft()) module.deleteBand(selectedBandId)
    }
    function undo() { if (manualEditable) { cancelDraft(); module.undo() } }
    function redo() { if (manualEditable) { cancelDraft(); module.redo() } }
    function graphDrag(hz, db) {
        if (!manualEditable || !loaded || draftField.length) return
        graphDraft={id: selectedBandId, hz: hz, gain: db}
    }
    function graphRelease() {
        if (!manualEditable || !graphDraft) return false
        const next=graphDraft
        graphDraft=null
        return module.editBandGraph(next.id,next.hz,next.gain)
    }
    function adjustSecondaryParameter(steps, fine) {
        if (!manualEditable || !loaded) return
        const field=qApplicable ? "q" : "shelfSlope"
        if (!qApplicable && !shelfSlopeApplicable) return
        const previous=draftField===field && draftBandId===selectedBandId
            ? Number(draftText.replace(",", ".")) : (qApplicable ? q : shelfSlope)
        if (!Number.isFinite(previous)) return
        const next=Math.max(0.1,Math.min(qApplicable?12:1,previous+steps*(fine?0.01:0.1)))
        setDraftFieldValue(field,Math.round(next*100)/100)
    }
    // The frozen C01 filter family has no cut-slope parameter.
    function setDraftSlopeDbPerOct(value) {}
}
