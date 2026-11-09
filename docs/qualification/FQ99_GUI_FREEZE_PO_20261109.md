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

## 9 novembre — #23 differential evidence; #24 lazy overlay correction
- CI #23 Linux PASS, Windows RED (10/11 CTest PASS, only `rgsml_source_metadata_panel_smoke` failed at actual 1184×688): frozen EQ 992×612 inside allocated host 732×475.
- **Definitive A/B evidence** bounded Windows artifact `13691332768`: the same candidate and exact Windows Qt runner, with ONLY the newly added `AnalysisStatusEditor` child deleted in a TEMP worktree, yields `M19_CHILD_AB: CANDIDATE_MINUS_NEW_CHILD_NATIVE_SMOKE=PASS`; original candidate still FAIL. The new hidden direct child was sufficient to break the qualified ColumnLayout size allocation.
- Fix only the M19–M21 addition: remove direct `AnalysisStatusEditor` child from the frozen DSP `ColumnLayout`, create **lazy sibling Loader overlay** after its closing brace and before the existing M17 popover; `active` only for editorContentKey `M19_ANALYSIS`, `M20_ANALYSIS`, `M21_ANALYSIS`. Its own anchors position it below the existing title strip without moving that strip. No FQ99 QML statement or qualified GUI geometry changed.
- Updated byte-for-byte FQ99 guard: after subtracting this sole explicitly reviewed sibling insertion and the existing M19–M21 wiring, `DspEditorHost.qml` equals FQ99 exactly, verified by independent GitHub content comparison. M19–M21 navigation and the approved new file remain; Master Audition stays present.
- Static product navigation regression guard now enforces the loader resides after the original Stereo/M-S child and before the pre-existing popover, and is lazy. No test exclusions or relaxation of minimum-size containment were made. Only a full new Windows CTest can confirm the regression is repaired. PR #99 remains Draft and no merge to main authorized.

## 9 novembre — Decisione di progetto: M16 è il riferimento per M19–M21
**Rettifica rispetto alla proposta di overlay esterno.** Il PO ha osservato correttamente che M16 True Peak Meter è già un modulo `non_processing_service()` inserito nella DSP Chain con il proprio editor nel `ColumnLayout` del `DspEditorHost`. M16 fu qualificato senza alterare Parametric EQ: non è legittimo sostenere che un analizzatore debba vivere necessariamente fuori dall'host.

Dettaglio delle differenze: `TruePeakMeterEditor` esistente ha `visible: root.isMeterEditor`, `Layout.fillWidth/fillHeight: true` e `Layout.minimumHeight/preferredHeight: 0`; il nuovo `AnalysisStatusEditor` dichiara `implicitWidth: 640`, `implicitHeight: 360`. Il test A/B #23 dimostra che istanziare il nuovo figlio, anche invisibile, contribuisce al fallimento; la successiva #24 mostra che un `Loader` esterno all'originale `ColumnLayout`, ma ancora figlio del `DspEditorHost`, non basta a eliminare la regressione. Non è dimostrato che **ogni** nuovo figlio del `ColumnLayout` fallisca: occorre un gate Qt reale.

**Nuovo intervento sperimentale e strettamente additivo, in attesa CI:** Loader interno nel medesimo `ColumnLayout` e nella medesima sequenza visiva di M16, con `active: analysisSelected` e `visible: analysisSelected`, `Layout.minimumHeight/preferredHeight: 0`, istanzia `AnalysisStatusEditor` **solo** quando è selezionato M19, M20 o M21. Non esiste dunque alcun editor di analisi con `implicitWidth/Height` quando è selezionato Parametric EQ/Compressor o altro DSP già qualificato. Nessun elemento del vecchio layout è alterato. Il comportamento è diverso da una semplice proprietà `visible: false` su un `Item` già creato.

Esplorazioni NON pubblicate che collocavano overlay in `Main.qml` o `DspFloatingModuleWindow.qml` sono state **revocate sul writer**: le due GUI non devono essere modificate per aggirare il problema. La guardia FQ99 verifica ora l'unico nuovo `Loader` M19–M21 e l'esatto contenuto qualificato esistente; static source guard verifica la laziness e il posizionamento fra M16 e Monitor. Nessuna CI di questa nuova soluzione può essere dichiarata superata senza esecuzione Windows/Linux. Se persiste il fallimento, evitare ogni nuova modifica a moduli già qualificati e isolare il contratto di allocazione Qt.

## 9 novembre — CI #25 fallita, inversione correttiva: isolare M19–M21 fuori da DspEditorHost
- #25 (https://github.com/casaferla/RGS-MasterLab/actions/runs/42658683605): Linux PASS, Windows 10/11 CTest PASS, solo `rgsml_source_metadata_panel_smoke` FAIL. Artifact Windows `13694221419`: editor Parametric EQ ancora 992x612 in host 732x475, dopo 50 iterazioni Qt. A/B stessa commit Windows, con rimozione SOLO del nuovo Loader M19–M21 nel ColumnLayout: `M19_CHILD_AB: CANDIDATE_MINUS_NEW_CHILD_NATIVE_SMOKE=PASS`. Confronto #22 FQ99 originale sullo stesso runner PASS.
- CONCLUSIONE BASATA SULL'EVIDENZA: la presenza di un nuovo oggetto QQuickItem/Loader nell'albero del vecchio DspEditorHost altera in questo contesto il contratto di layout alla finestra minima. Il fatto che M16 sia stato integrato come figlio del ColumnLayout resta vero e va rispettato: M16 e le altre GUI non si riscrivono. Non si generalizzi a tutti gli analizzatori, ma non si introduca altro layout child nel componente congelato.
- Candidato successivo: `DspEditorHost.qml` ripristinato byte-per-byte alla FQ99 dopo sottrazione della sola property/condizione M19–M21; nessun nuovo figlio ne' Loader. Nuovo Loader M19–M21 invece collocato nel dock-slot della `Main.qml`, esterno al vecchio host e al suo LayoutItemProxy. Istanzia AnalysisStatusEditor unicamente quando è selezionato M19/M20/M21, senza riposizionare titolo, toolbar o pannelli originali. La vecchia `DspFloatingModuleWindow.qml` è byte-identica a FQ99.
- Protezioni aggiornate: source guard `guard_fq99_frozen_gui.py` sottrae virtualmente soltanto l'esatta addizione Main e il wiring approvato; guard prodotto impone assenza di QQuickItem nuovo nell'host. Independently verified: DspEditorHost, Main, DspWorkspace, DspChainSelector ripristinabili **esattamente** alla FQ99; DspFloatingModuleWindow immutata. Il test Qt nativo NON deve essere escluso o rilassato. La visualizzazione floating di M19–M21 non è ancora qualificata e resta gate successivo.
- Questa modifica è un'ipotesi tecnica fortemente supportata dall'A/B ma ancora **non una soluzione qualificata** finché Windows CI e FQ fisica non saranno superate. Nessun merge `main`, PR #99 Draft.
