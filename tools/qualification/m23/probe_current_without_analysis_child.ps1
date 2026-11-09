# Nonrelease, nonauthorizing A/B experiment. Run EXACT current source in
# a detached TEMP worktree, but remove ONLY the new M19/M20/M21
# sibling Loader overlay. All FQ99-qualified QML and the original
# candidate remain intact, and candidate failure stays fatal.
param([Parameter(Mandatory=$true)][string]$QtRoot,
      [Parameter(Mandatory=$true)][string]$BoundedEvidence)
$ErrorActionPreference="Continue"
$src=Join-Path $env:RUNNER_TEMP "rgsml-ab-no-analysis-child"
$build=Join-Path $env:RUNNER_TEMP "rgsml-ab-no-analysis-build"
$setup=Join-Path $env:RUNNER_TEMP "rgsml-ab-no-analysis-setup.txt"
$log=Join-Path $env:RUNNER_TEMP "rgsml-ab-no-analysis-ui-log.txt"
$stdout=Join-Path $env:RUNNER_TEMP "rgsml-ab-no-analysis-ui-console.txt"
function Note([string]$s) {
    Add-Content -Path $BoundedEvidence -Value ("M19_CHILD_AB: "+$s)
}
Note "CANDIDATE_SHA=$env:GITHUB_SHA"
Note "EXPERIMENT=REMOVE_NEW_M19_M20_M21_LAZY_OVERLAY_ONLY_IN_TEMP_WORKTREE"
& git worktree add --detach $src $env:GITHUB_SHA *> $setup
if($LASTEXITCODE -ne 0) {Note "INCONCLUSIVE_WORKTREE_FAILED";exit 0}
$p=Join-Path $src "ui/qml/DspEditorHost.qml"
$s=[System.IO.File]::ReadAllText($p)
$start='    // M19–M21 are independent analysis surfaces, NOT ColumnLayout children.'
$finish='    // M17 popover uses the SAME copper/teal family rule'
$idx=$s.IndexOf($start,[System.StringComparison]::Ordinal)
if($idx -lt 0) {Note "INCONCLUSIVE_NEW_CHILD_NOT_FOUND";exit 0}
$end=$s.IndexOf($finish,$idx,[System.StringComparison]::Ordinal)
if($end -lt 0 -or $s.IndexOf($start,$idx+1,[System.StringComparison]::Ordinal) -ge 0) {
    Note "INCONCLUSIVE_NEW_CHILD_AMBIGUOUS";exit 0
}
$removed=$s.Substring($idx,$end-$idx)
if(!$removed.Contains('objectName: "m19M21ProductAnalysisOverlay"') -or
   !$removed.Contains('AnalysisStatusEditor {') -or
   !$removed.Contains('active: analysisSelected')) {
    Note "INCONCLUSIVE_UNEXPECTED_CHILD_CONTENT";exit 0
}
$s=$s.Remove($idx,$end-$idx)
[System.IO.File]::WriteAllText($p,$s,[System.Text.UTF8Encoding]::new($false))
Note "TEMP_CHILD_BLOCK_REMOVED_BYTES=$($removed.Length)"
# A temporary baseline is permitted for scientific isolation, never for
# replacing, shipping or silently editing the approved GUI.
& cmake -S $src -B $build -G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Release -DRGSML_BUILD_TESTS=ON -DRGSML_WARNINGS_AS_ERRORS=ON "-DRGSML_QT_ROOT=$QtRoot" *> $setup
if($LASTEXITCODE -ne 0) {Note "INCONCLUSIVE_CONFIGURE_FAILED";Get-Content $setup -Tail 3 | ForEach-Object {Note ("CONFIG: "+$_)};exit 0}
& cmake --build $build --config Release --parallel 4 --target rgsml_source_metadata_panel_smoke *> $setup
if($LASTEXITCODE -ne 0) {Note "INCONCLUSIVE_BUILD_FAILED";Get-Content $setup -Tail 3 | ForEach-Object {Note ("BUILD: "+$_)};exit 0}
$exe=Join-Path $build "Release/rgsml_source_metadata_panel_smoke.exe"
if(!(Test-Path $exe)) {Note "INCONCLUSIVE_EXE_MISSING";exit 0}
$env:PATH="$QtRoot/bin;$env:PATH"
$env:QML2_IMPORT_PATH="$QtRoot/qml"
Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue
$env:RGSML_GUI01_EVIDENCE_DIR=Join-Path $env:RUNNER_TEMP "rgsml-ab-no-analysis-images"
New-Item -ItemType Directory -Force -Path $env:RGSML_GUI01_EVIDENCE_DIR | Out-Null
& $exe emptyReadyErrorAndWindowLifecycle -v2 -o "$log,txt" *> $stdout
$code=$LASTEXITCODE
if($code -eq 0) {
    Note "CANDIDATE_MINUS_NEW_CHILD_NATIVE_SMOKE=PASS"
} else {
    Note "CANDIDATE_MINUS_NEW_CHILD_NATIVE_SMOKE=FAIL EXIT=$code"
    if(Test-Path $log){
        @(Select-String -Path $log -Pattern 'FAIL!|QFATAL|Fatal error' |
            Select-Object -First 4) | ForEach-Object {
            $line=$_.Line
            if($line.Length -gt 240){$line=$line.Substring(0,240)}
            Note ("FAIL:"+$line)
        }
    } else { Note "NO_QT_SMOKE_LOG" }
}
# Candidate CTest result stays RED. Never use this experiment as a gate PASS.
exit 0
