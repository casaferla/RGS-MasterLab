# RGS MasterLab — PO GUI FREEZE, 9 novembre 2026

**Stato: DECISIONE PO VINCOLANTE.** Le GUI già qualificate sono congelate. Nessuna modifica a geometrie, dimensioni, disposizione, testi, caratteri, colori, pulsanti, navigazione o comportamento dei moduli qualificati può essere introdotta come scorciatoia per rendere verde una CI di un modulo successivo.

## Baseline immutabile
- **FQ #99 verde**, commit sorgente: `5e4871d5849e4121b0612c2142d1e051d6c9a3d2`.
- Parametric EQ `ui/qml/ParametricEqEditor.qml`: blob originale `a3a1aec60b7afbe57734340f743f3faafdf8273c`, ripristinato.
- True Peak Meter M16 e suo rail condiviso M17 `ui/qml/TruePeakMeterEditor.qml`: blob originale `1aa406aee2d4a3ba4b4223cd246e5f52fb7b5ce6`, ripristinato.
- `DspEditorHost.qml`: annullata la corsia forzata 188 DIP del titolo e ripristinati i `minimumControlWidth: 86` FQ per Prepare/Cancel.
- `DspChainSelector.qml`: ripristinata l'originale tipografia/title-elision del sidebar.
- `Main.qml`: rimossi Shortcut Ctrl+O e barra diagnostica Export introdotti dopo la FQ, perché non indispensabili a M19–M21 e non coperti dalla baseline. La causa dell'Export non deve essere occultata: permane un problema da diagnosticare senza mutare il layout qualificato.
- FQ #99 vincola anche tutti gli altri componenti originali (Input Gain, EQ, Compressor, Stereo/M-S, M16, M17, Master Audition, StudioButton ecc.) e i layout condivisi.

## Eccezione strettamente additiva per M19–M21
Le sole differenze nei vecchi file di GUI sono gli **innesti già autorizzati** per i nuovi moduli, senza riscritture di elementi esistenti:
- `DspChainSelector.qml`: tre voci di navigazione M19/M20/M21 (icone definitive ancora da recuperare da Drive; non improvvisare).
- `DspEditorHost.qml`: property, routing per tre editor nuovi e un nuovo `AnalysisStatusEditor` visibile solo sui corrispondenti moduli.
- `DspWorkspace.qml` e `Main.qml`: wiring per lo status read-only.
- `ui/qml/AnalysisStatusEditor.qml`: file nuovo e isolato.
- `ui/CMakeLists.txt`: registrazione del nuovo QML.

**Per nuove funzioni/UI:** collocare eventuali nuovi pulsanti in spazi disponibili, con logica funzionale coerente. Non spostare, ridurre, sovrapporre, riciclare, sostituire o nascondere i controlli qualificati; non usare tooltip per giustificare testi tagliati. Una modifica a una GUI FQ si discute col PO, viene isolata, documentata e deve superare nuovamente gate di regressione e FQ appropriata.

## Regola CI
- `tools/qualification/m23/guard_fq99_frozen_gui.py` confronta ogni file QML preesistente con la revisione FQ #99: differenze ammesse solo nei nuovi innesti esatti sopra elencati, sottratti virtualmente prima del confronto **byte-per-byte**. Qualsiasi altra modifica fa fallire la CI.
- Il guard gira in **Windows e Linux** nel source-contract M23. Non sostituisce Qt CTest, FQ o verifica fisica, e non dichiara qualificata l'aggiunta M19–M21.
- Non modificare gli expected blob, la baseline FQ o l'allowlist per far passare una CI senza ulteriore decisione PO.

## Residui e separazioni
- #17 era Windows RED / Linux GREEN: il Qt CTest riportava `parametricEqEditor must stay inside dspEditorHost at minimum size`. La causa del vincolo di layout va verificata senza riscrivere Parametric EQ o altri DSP qualificati; confrontare pipeline UI e backend e aggiunte M19–M21.
- Nessun merge su `main`; PR #99 Draft; tutte le qualificazioni fisiche/PO richieste restano aperte.
