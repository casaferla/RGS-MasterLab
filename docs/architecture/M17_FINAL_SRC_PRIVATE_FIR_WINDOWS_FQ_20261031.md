# M17 — original Q0 FIR PRIVATE local installation for physical Windows FQ
This is a developer/operator fixture installation only. **No raw FIR asset goes to public GitHub, CI artifacts, a log or a downloaded build.** Source is the exact RGS ContentBox private original ZIP SHA256 `94d3340667ab5c86b2db5f2d0e191f58b5d05863413312fb3545faed51798ce4`; never substitute Phase2/Phase3 receipt ZIPs.

On the authorized Windows FQ machine, obtain the original private ZIP from RGS ContentBox. With Python 3.12:

```powershell
py -3.12 tools/qualification/final_src/install_private_q0_originals.py --original-private-zip "C:\\Secure\\RGS_Phase11_14_Q0_candidate_manifest_and_assets_NONRELEASE_2026-10-30.zip" --new-directory "C:\\Secure\\RGSML_FIR_Original14"
$env:RGSML_FINAL_SRC_Q0_PRIVATE_DIR = "C:\\Secure\\RGSML_FIR_Original14"
```

Launch RGS MasterLab in that SAME environment. The product reads ONLY this explicit local directory at startup; verifies the full 14/14 original SHA/geometry, and separately verifies the target FIR SHA each Prepare. It never contacts Drive/cloud while running. Without this exact private installation, 28 new conversions remain blocked. Source and DEVICE rates are independent of MASTER. Active Loop Region and PLAY/PAUSE do not prevent full-track offline Prepare. The installer does **not** authorize a release, Q1–Q6 global PASS, physical FQ or `main` merge.

Windows FQ must still be performed on a single immutable built SHA after Focused Windows/Linux green, with real audibility, 1184x688 at 150%, actual 36 SOURCE→MASTER conversions (6 identity + 30 real), M17/M16 exact MASTER, -12 dBTP regression, full-track RAM/time/cancel and DEVICE independence; collect bounded screenshots/report only. The public GitHub Actions CI does **not** have the private 14 FIRs by default, so a CI green is never misrepresented as private 36-route execution.
