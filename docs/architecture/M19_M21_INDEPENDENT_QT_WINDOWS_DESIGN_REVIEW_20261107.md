# M19–M21 independent analysis panels · design-only checkpoint · 2026-11-07

The Windows Qt6 focused screenshot #6 at https://github.com/casaferla/RGS-MasterLab/actions/runs/42418075113 is SUCCESS, with 12 real PNGs comparing unified and individual M19 Levels, M20 Tonal/Stereo, M21 Metrics/Delta at 1440×900 and 1184×688. All values are fictional DEMO DATA.

Review proposal (NOT formal PO sign-off): prefer three independent dockable/detachable panels for concurrent reference work, with possible grouped ANALYSIS tabs as an optional compact view. This work tests three separate simultaneously live QQuickView instances, not only tab switching within one view. Each wrapper is separately instantiated and fixed to its own module; the shared visual implementation avoids duplicated source QML.

Out of scope: Main.qml integration, production dock/float lifecycle, separate owner/cache, saved arrangement, authentication of PREPARED/GOLD/PROCESSED, status/epoch gates, real Qt presenter, and release. Existing DSP, M16/M17/FIR, M18 Export and M19–M22 engines remain untouched. This screenshot-only Windows CI is not Full Qualification, Linux analysis testing or Windows physical PO acceptance.

Must complete in subsequent authorized phase: native Main-owned floating windows with minimum size equal to docked content, source transition cleanup, validated Product bindings, stale/UNAVAILABLE truthfulness, no GOLD-alignment fiction, numerical/status tests, 4K+FHD, resize/dock/redock acceptance, M18 A01–A15, final Gates and end-to-end listening. No merge to main without explicit PO authorization.
