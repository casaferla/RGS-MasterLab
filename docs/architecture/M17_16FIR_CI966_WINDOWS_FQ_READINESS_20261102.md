# M17 / 16 FIR — Focused CI #966 e FQ Windows readiness (2026-11-02)

Status: **FOCUSED WINDOWS+LINUX PASS; NUOVA WINDOWS COLD FQ NON ANCORA ESEGUITA**.

## Baseline immutabile
- Repository: `casaferla/RGS-MasterLab`.
- PR #61: Draft, unmerged; branch `experimental/m17-monitoring-pcm-isolation-20261024`.
- PR SHA Focused: `6e5171e0efcbc07b5bace97f3b3975798aea4f57`.
- Staging al controllo pre-checkpoint: `staging/m17-monitor-preallocation-no-ci-20261024`, identical / ahead 0 / behind 0.
- Focused #966: https://github.com/casaferla/RGS-MasterLab/actions/runs/41403116545; Linux Portable DSP Guard SUCCESS e Windows Focused DSP SUCCESS, entrambi su SHA 6e5171e0.
- L'ultima Full Qualification manuale eseguita al momento dell'audit resta FQ #88 (SHA `7e1da1156928390816541d34f35bbc6e192dc5a0`), non valida per questo successivo codice.

## Asset e package
- `resources/fir/`: 14 binari originali `RGSMSR1_L*_M*.bin` importati byte-exact come normali file Git; 2 Playback storici restano estratti bit-exact da sorgenti C++ congelate. **16 binari al deploy**, stesso percorso relativo `bin/resources/fir` e un `fir_manifest.json`.
- Il Focused #966 esegue su entrambi gli OS `tools/qualification/playback_src/test_approved_16_fir_package.py`, comprendente prova positiva sui 14 veri asset, SHA-pin, test di tamper e rifiuto di sorgente FQ incompleta. Non equivale alla Full FQ installata.
- Il workflow manuale `.github/workflows/windows-full-qualification.yml` include, dopo installazione, il controllo `Verify installed 16 authentic FIR files in one flat package` con `verify_installed_16_firs.py`. Pacchetto autosufficiente: nessuna installazione FIR separata richiesta all'utente. L'archivio storico ZIP resta prova privata su ContentBox, non dipendenza di build.
- La qualifica matematica Q0 e la disponibilità del file non promuovono autonomamente le 28 tratte DEVICE sperimentali: gate Q1–Q6/performance real-time/physical owner e M17 restano distinti.

## Prossima azione sottoposta al Product Owner
Il Product Owner, **non** la chat operativa, può avviare una sola nuova Full Qualification Windows manuale:
- Workflow: `Windows Full Qualification`.
- Branch: `experimental/m17-monitoring-pcm-isolation-20261024`.
- `expected_source_sha=6e5171e0efcbc07b5bace97f3b3975798aea4f57`.
- `cache_mode=cold`.

Se verde, verificare artifact ZIP completo, esatto inventario 16/16 e test fisici Windows 150% DPI, one Region, Source/Master/Device transition, vero Prepare 36-route as applicable, meters, tempistiche, ascolto e M17 full-song ceiling/tp. Se rosso, correggere su staging in macro-lotto, poi CAS una sola promozione PR e nuova CI. **No merge main; M18 non iniziato.** Dopo accettazione M17 vale il precedente mandato PO di audit/reset architettura CI prima di M18.

## Disciplina operativa
Il presente checkpoint è una modifica esclusivamente documentale sul branch staging senza PR e senza nuova CI; **non altera** lo SHA 6e5171e0 precedentemente qualificato. Prima di ogni altra scrittura rileggere `AGENTS.md`, `docs/architecture/OPERATIONAL_HANDOFF_CONTRACT.md`, `docs/architecture/ACTIVE_OPERATOR_STATE.md` e gli HEAD GitHub correnti. Conservare Single-Writer Epoch e non interpretare la modifica docs-only come testata da #966.
