# M23 GUI A/B MATRIX — diagnostic only; not a CI qualification gate.
# FQ99 vs candidate, FULL Qt smoke suite, screenshots explicitly OFF/ON.
# Each arm is a fresh native Windows process; never mutate production QML.
# The old single-case + screenshot ablation was NOT a controlled A/B.
param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$BoundedReceipt
)
$ErrorActionPreference = "Continue"
$baselineSha = "5e4871d5849e4121b0612c2142d1e051d6c9a3d2"
$expectedCandidate = "ba8bf9ea41b4d4e950bb780f035a368f1f1a10c9"
$baseSrc = Join-Path $env:RUNNER_TEMP "rgsml-ab-fq99-source"
$baseBuild = Join-Path $env:RUNNER_TEMP "rgsml-ab-fq99-build"
$candSrc = Join-Path $env:RUNNER_TEMP "rgsml-ab-cand27-source"
$candBuild = Join-Path $env:RUNNER_TEMP "rgsml-ab-cand27-build"
$work = Join-Path $env:RUNNER_TEMP "rgsml-ab-controlled"
New-Item -ItemType Directory -Path $work -Force | Out-Null
Set-Content -Path $BoundedReceipt -Value @(
    "RGSML_CONTROLLED_QT_GUI_MATRIX=DIAGNOSTIC_ONLY",
    "BASELINE_SHA=$baselineSha",
    "CANDIDATE_SOURCE_SHA=$expectedCandidate",
    "DIAGNOSTIC_RUN_SHA=$env:GITHUB_SHA",
    "BOTH_REVISIONS_INDEPENDENT_CLEAN_BUILDS=YES",
    "MATRIX=FULL_SUITE_NATIVE_WINDOWS_SAME_ARGS",
    "FQ99_NOT_AUTOQUALIFIED=YES",
    "ORIGINAL_CTEST_GATE_UNCHANGED=YES"
)
function Note([string]$line) {
    Add-Content -Path $BoundedReceipt -Value $line
}
function Short([string]$line,[int]$limit=230) {
    $line = $line -replace '[\r\n]+', ' '
    if($line.Length -gt $limit) {return $line.Substring(0,$limit)}
    return $line
}
function BuildSnapshot([string]$tag,[string]$sha,[string]$src,[string]$build) {
    $setup=Join-Path $work "$tag-build.log"
    & git worktree add --detach $src $sha *> $setup
    if($LASTEXITCODE -ne 0) {
        Note "BUILD_$tag=INCONCLUSIVE_WORKTREE_ERROR"
        if(Test-Path $setup) {Get-Content $setup -Tail 4 | ForEach-Object {Note ("BUILD_ERR_$tag="+(Short $_))}}
        return ""
    }
    $actual=(& git -C $src rev-parse HEAD)
    Note "SOURCE_$tag=$actual"
    if($actual -ne $sha) {
        Note "BUILD_$tag=INCONCLUSIVE_SHA_MISMATCH"
        return ""
    }
    & cmake -S $src -B $build -G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Release -DRGSML_BUILD_TESTS=ON -DRGSML_WARNINGS_AS_ERRORS=ON "-DRGSML_QT_ROOT=$QtRoot" *> $setup
    if($LASTEXITCODE -ne 0) {
        Note "BUILD_$tag=INCONCLUSIVE_CONFIGURE_ERROR"
        Get-Content $setup -Tail 4 | ForEach-Object {Note ("BUILD_ERR_$tag="+(Short $_))}
        return ""
    }
    & cmake --build $build --config Release --parallel 4 --target rgsml_source_metadata_panel_smoke *> $setup
    if($LASTEXITCODE -ne 0) {
        Note "BUILD_$tag=INCONCLUSIVE_COMPILATION_ERROR"
        Get-Content $setup -Tail 4 | ForEach-Object {Note ("BUILD_ERR_$tag="+(Short $_))}
        return ""
    }
    $exe=Join-Path $build "Release/rgsml_source_metadata_panel_smoke.exe"
    if(!(Test-Path $exe)) {
        Note "BUILD_$tag=INCONCLUSIVE_EXE_MISSING"
        return ""
    }
    Note "BUILD_$tag=OK"
    return $exe
}
function RunArm([string]$label,[string]$exe,[bool]$screenshots) {
    if(!(Test-Path $exe)) {
        Note "ARM=$label STATUS=INCONCLUSIVE REASON=EXE_MISSING"
        return
    }
    $log = Join-Path $work "$label-qt-full.txt"
    $std = Join-Path $work "$label-stdio.txt"
    Remove-Item $log,$std -ErrorAction SilentlyContinue
    if($screenshots) {
        $snap = Join-Path $work "$label-captures"
        New-Item -ItemType Directory -Path $snap -Force | Out-Null
        $env:RGSML_GUI01_EVIDENCE_DIR = $snap
    } else {
        Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
    }
    $photo = if($screenshots) {"ON"} else {"OFF"}
    Note "ARM_START=$label SCREENSHOTS=$photo MODE=FULL_QT_SUITE"
    $started = Get-Date
    try {
        # SAME exact CLI arguments for all four arms; never select one test.
        & $exe -v2 -o "$log,txt" *> $std
        $code=$LASTEXITCODE
    } catch {
        $code=-999
        Note ("ARM_EXCEPTION=$label DETAILS="+(Short $_.Exception.Message))
    }
    $seconds=[math]::Round(((Get-Date)-$started).TotalSeconds,1)
    if(!(Test-Path $log)) {
        Note "ARM=$label STATUS=INCONCLUSIVE EXIT=$code SCREENSHOTS=$photo LOG=MISSING ELAPSED_S=$seconds"
        if(Test-Path $std) { Get-Content $std -Tail 2 | ForEach-Object {Note ("STDIO: "+(Short $_))} }
        return
    }
    $fail = @(Select-String -Path $log -Pattern 'FAIL!|QFATAL|FATAL' | Select-Object -First 3)
    $totals = @(Select-String -Path $log -Pattern '^Totals: ' | Select-Object -Last 1)
    $status = if($code -eq 0 -and $fail.Count -eq 0) {"PASS"} else {"FAIL"}
    Note "ARM=$label STATUS=$status EXIT=$code SCREENSHOTS=$photo ELAPSED_S=$seconds"
    if($totals.Count -gt 0) {Note ("ARM_TOTALS=$label "+(Short $totals[0].Line))}
    foreach($line in $fail) {Note ("ARM_FIRST_FAILURE=$label "+(Short $line.Line))}
}
$workSha = (& git rev-parse HEAD)
Note "WORKTREE_SHA=$workSha"
Note ("CANDIDATE_SHA_MATCH="+($workSha -eq $env:GITHUB_SHA))
Note "QT_ROOT=$QtRoot"
$qmake=Join-Path $QtRoot "bin/qmake.exe"
if(Test-Path $qmake) {
    $qtVersion=(& $qmake -query QT_VERSION | Select-Object -First 1)
    Note "QT_RUNTIME_VERSION=$qtVersion"
}
Note "QPA_PLATFORM=UNSET;QT_QUICK_BACKEND=UNSET"
Note "EXPLICIT_SCREENSHOT_VAR_CONTROL=YES"
Note "SAME_CWD=$env:GITHUB_WORKSPACE"
$env:PATH="$QtRoot/bin;$env:PATH"
$env:QML2_IMPORT_PATH="$QtRoot/qml"
Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue
Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
Push-Location $env:GITHUB_WORKSPACE
try {
    # B1/B2 are identical immutable CANDIDATE #27 and same clean binary.
    $candidateExe=BuildSnapshot "B" $expectedCandidate $candSrc $candBuild
    if($candidateExe -and (Test-Path $candidateExe)) {
        RunArm "B1_candidate_off" $candidateExe $false
        RunArm "B2_candidate_on"  $candidateExe $true
    } else {
        Note "ARMS_B1_B2=INCONCLUSIVE_NOT_RUN"
    }
    # A1/A2 use exact immutable FQ99, with identical clean build settings.
    $baselineExe=BuildSnapshot "A" $baselineSha $baseSrc $baseBuild
    if($baselineExe -and (Test-Path $baselineExe)) {
        RunArm "A1_fq99_off" $baselineExe $false
        RunArm "A2_fq99_on"  $baselineExe $true
    } else {
        Note "ARMS_A1_A2=INCONCLUSIVE_NOT_RUN"
    }
    if($candidateExe -and (Test-Path $candidateExe)) {
        # Re-run B1 after other arms to detect order/caching effects.
        RunArm "B1_repeat_candidate_off" $candidateExe $false
    }
} finally {
    Pop-Location
    Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
}
Note "MATRIX_COMPLETE=YES"
# Always zero: exploratory findings cannot certify or greenwash original CTest.
exit 0
