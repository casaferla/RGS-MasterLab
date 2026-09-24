# RGS MasterLab — L1-M12B Visual WOW Frozen Authority Package

Status: PRODUCT-OWNER FROZEN DESIGN AUTHORITY — DOCUMENTATION-ONLY BRANCH.

This branch exists only so the existing Jules session can read the frozen M12B Visual WOW authority without changing Draft PR #8.

## Rules

- DO NOT merge or cherry-pick this branch into PR #8.
- DO NOT switch the implementation worktree away from the existing PR branch.
- Read/extract the ZIP from this branch into a temporary directory only.
- Implementation branch remains `draft/m12b-parametric-eq-editor-window-9003884410820853273`.
- Authoritative pre-WOW PR HEAD: `5676505f586738d29349b3a2f4907ba011575141`.
- This package is documentation/design authority only. It does not authorize edits outside the bounded QML implementation handoff.

## Frozen package

`RGS_MasterLab_L1-M12B_WOW_FROZEN_2026-09-24.zip`

Drive source ID: `1OOpHadntY3uebTvi0brxJ8SVrGsQvqD3`

Expected ZIP SHA-256:
`7ce2251c93e0e581c6aa8cd07b0955d0e4882d8e9762a4c8a2fe74db8ca1d41f`

The ZIP contains:
- MANIFEST_SHA256.json
- README_FIRST.txt
- PO freeze decision
- consolidated eight-board authority index
- Jules implementation handoff
- Jules copy/paste prompt
- all eight canonical Board01–Board08 manifests
- Board01 and Board08 visual references
- pre-WOW real Windows 1040x660 visual reference

## Read-only access from the existing PR worktree

```bash
git fetch origin handoff/m12b-wow-frozen-authority
git show origin/handoff/m12b-wow-frozen-authority:docs/handoffs/m12b-wow-frozen/RGS_MasterLab_L1-M12B_WOW_FROZEN_2026-09-24.zip > /tmp/RGS_MasterLab_L1-M12B_WOW_FROZEN_2026-09-24.zip
sha256sum /tmp/RGS_MasterLab_L1-M12B_WOW_FROZEN_2026-09-24.zip
rm -rf /tmp/rgsml_m12b_wow_authority
mkdir -p /tmp/rgsml_m12b_wow_authority
unzip -q /tmp/RGS_MasterLab_L1-M12B_WOW_FROZEN_2026-09-24.zip -d /tmp/rgsml_m12b_wow_authority
```

Verify the SHA before using the extracted authority.

END — documentation-only authority branch.
