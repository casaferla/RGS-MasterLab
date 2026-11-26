# RGS MasterLab — preset file extensions and scope (2026-11-26)

The project container uses **`.rgsml`** (the canonical project extension; not a preset). Earlier Product preset dialogs used **`.rgspreset`** for both a single module and the entire manual DSP chain. The previous JSON schema already distinguished these via `scope: "MODULE"` versus `scope: "MANUAL_CHAIN"`, so this is a file-name/UX separation, **not** a new DSP codec or schema.

## Canonical preset extensions

| User operation | New default file extension | Existing JSON scope |
| --- | --- | --- |
| Save/Open one editable MANUAL DSP module | `.rgsdsp` | `MODULE` |
| Save/Open the entire MANUAL DSP Chain | `.rgschain` | `MANUAL_CHAIN` |
| Open an older preset | `.rgspreset` | Validate the embedded scope |
| Open an older JSON export | `.json` | Validate the embedded scope |

Both new file types use the same versioned `rgsml.dsp.manual-preset/1.0.0` JSON parameter codec, type IDs, algorithm/schema version checks and exact topology validation. New Save operations require the correct extension. Open operations reject a new extension for the wrong scope **before reading**; legacy `.rgspreset` or `.json` is allowed in either dialog but is applied only if its internal `scope` and the current MANUAL DSP topology match exactly. Never silently convert or override the embedded scope.

No automatic project commit, no REF_LINKED overwrite, no M12 AutoMatch authorization, and no new DSP instance allocation. The distinction applies to **DSP modules** and the **manual DSP Chain**; non-DSP analysis and transport modules are not made writable by this extension change.

## Windows acceptance

1. Save one editable DSP module, verify `.rgsdsp` and `MODULE` in the file.
2. Save the manual chain, verify `.rgschain` and `MANUAL_CHAIN`.
3. Open each with its own dialog; verify explicit confirmation and exact typed parameters.
4. Try the wrong new extension in each dialog; verify rejection with no state change.
5. Open a legacy `.rgspreset` in the correct scope; reject a scope mismatch with no state change.
6. Verify atomic save, explicit MANUAL restore, preview refresh and history semantics.
