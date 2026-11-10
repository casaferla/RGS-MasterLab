# M19–M21 Product readiness invalidation — 10 November 2026

**Scope:** nonrelease Product status synchronization. No DSP, audio, mastering, SRC, QML layout, persistence or Phase-6 authorization change.

## Source of truth
Product status is captured by `M21ProductQtReadinessBridge::capture`, through the authentic M18 epoch and existing M21 private inbox. The UI receives *quality and lifecycle only*, with `productionAuthorized = false`. The status ViewModel must neither synthesize numeric measurements nor own analysis worker threads.

## Existing observers preserved
`AuditionSourceSelector::changed`, `GoldSelectionViewModel::changed`, `ProjectSessionViewModel::changed`, `MasteringPreviewController::changed` continue to refresh the read-only status via queued connections. These connections were already present in the last double-green #48 baseline.

## Additional invalidation paths
- Direct committed `SourceSelectionViewModel::sourceChanged` (even if Audition routing does not independently notify).
- `MasterOutputRateState::changed` (clock/revision changes can retire a captured analysis without a file-picker event).
- `TerminalLimiterViewModel::acceptedStateChanged` (M17 candidate/terminal mutation can obsolete Product scope).
- All additions use `Qt::QueuedConnection` so that the status capture occurs after an active M18 synchronous mutation transaction has unwound.

## Bounded UI repaint
`M21ProductQtReadinessBridge::same_visible_frame` compares *every* owner identity, epoch, panel routing revision, three selected sources, per-column resource/generation/scope/rate/lifecycle receipts, and the complete per-panel QVariantMaps (including quality and reason). `M21ProductStatusViewModel::refresh` computes the new authentic frame but emits `changed` only when that complete visible state differs. This does **not** skip any Product epoch invalidation and is not a new Product permission.

## Native Qt gate
Seven added Qt smoke slots compare identical/changed frames, master rate and chain revisions, SOURCE/Project/Master generations, independent REFERENCE scope/sample rate, selected panel and evidence-quality transitions. Existing real Main geometry/EQ containment tests remain unchanged.

## Boundaries
A green focused CI is not a Windows physical qualification or full FQ. No change to seven frozen icons or M19–M21 icon vectors, no screenshot-only gate, no M18 Export consent change. No official Phase-6 FeatureDefinitionRegistry or release gate G exists here; Product numeric display remains masked until separately authorized and qualified.
