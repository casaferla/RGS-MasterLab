# M23 GUI phase-2: invocation-context isolation, read-only diagnostic.
# Exact SAME compiled binary, screenshot OFF. Only working directory /
# CMake wrapper varies. Never changes a product QML, test or CTest assertion.
param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$OriginalBuild,
    [Parameter(Mandatory=$true)][string]$CleanCandidateBuild,
    [Parameter(Mandatory=$true)][string]$BoundedReport
)
$ErrorActionPreference = "Continue"
$scriptRoot = Join-Path $env:GITHUB_WORKSPACE "tests/cmake/m16_c2_smoke_ctest_diagnostic.cmake"
$logs = Join-Path $env:RUNNER_TEMP "rgsml-context-comparison"
New-Item -ItemType Directory -Path $logs -Force | Out-Null
Set-Content $BoundedReport -Value @(
    "M23_GUI_INVOCATION_CONTEXT_DIAGNOSTIC=YES",
    "SOURCE_CANDIDATE_SHA=ba8bf9ea41b4d4e950bb780f035a368f1f1a10c9",
    "SCREENSHOT_VARIABLE=EXPLICITLY_OFF",
    "QPA_AND_BACKEND_OVERRIDES=UNSET",
    "SAME_EXE_PAIR_FOR_ROOT_AND_CTEST_CWD=YES",
    "NATIVE_UI_ASSERTIONS_UNMODIFIED=YES"
)
function Note([string]$msg) { Add-Content -Path $BoundedReport -Value $msg }
function Clip([string]$s) {
    $s = $s -replace '[\r\n]+',' '
    if($s.Length -gt 260){return $s.Substring(0,260)}
    return $s
}
$env:PATH="$QtRoot/bin;$env:PATH"
$env:QML2_IMPORT_PATH="$QtRoot/qml"
Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue
Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
$root=$env:GITHUB_WORKSPACE
function Run([string]$tag,[string]$exe,[string]$cwd,[bool]$wrapped) {
    if(!(Test-Path $exe) -or !(Test-Path $cwd)) {
        Note "ARM=$tag STATUS=INCONCLUSIVE MISSING_EXE_OR_CWD"
        return
    }
    $log=Join-Path $logs "$tag-full-qt.txt"
    $stdio=Join-Path $logs "$tag-stdio.txt"
    Remove-Item $log,$stdio -ErrorAction SilentlyContinue
    Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
    Push-Location $cwd
    try {
        if($wrapped) {
            # Mirrors tests/CMakeLists.txt CTest wrapper, without modifying it.
            & cmake "-DRGSML_SMOKE_EXE=$exe" -P $scriptRoot *> $stdio
            $exitCode=$LASTEXITCODE
            $wrapperLog=Join-Path $cwd "m16_c2_ctest_smoke_second_invocation.txt"
            if(Test-Path $wrapperLog) {
                Copy-Item $wrapperLog $log -Force
            }
        } else {
            & $exe -v2 -o "$log,txt" *> $stdio
            $exitCode=$LASTEXITCODE
        }
    } catch {
        $exitCode=-998
        Note ("ARM_ERROR=$tag "+(Clip $_.Exception.Message))
    } finally {
        Pop-Location
    }
    $text = if(Test-Path $log) {
        @(Select-String -Path $log -Pattern 'FAIL!|QFATAL|FATAL' | Select-Object -First 3)
    } else { @() }
    $total=if(Test-Path $log) {
        @(Select-String -Path $log -Pattern '^Totals: ' | Select-Object -Last 1)
    } else { @() }
    $status=if(!(Test-Path $log)){"INCONCLUSIVE"}
        elseif($exitCode -eq 0 -and $text.Count -eq 0){"PASS"} else {"FAIL"}
    Note "ARM=$tag STATUS=$status EXIT=$exitCode WORKDIR=$cwd WRAPPED=$wrapped"
    if($total.Count) {Note ("TOTAL_$tag="+(Clip $total[0].Line))}
    foreach($x in $text) {Note ("FAIL_$tag="+(Clip $x.Line))}
    if($status -eq "INCONCLUSIVE" -and (Test-Path $stdio)){
        Get-Content $stdio -Tail 3 | ForEach-Object {Note ("STDIO_$tag="+(Clip $_))}
    }
}
$originalExe=Join-Path $OriginalBuild "Release/rgsml_source_metadata_panel_smoke.exe"
$cleanExe=Join-Path $CleanCandidateBuild "Release/rgsml_source_metadata_panel_smoke.exe"
foreach($pair in @(@("M23_GATE_BINARY",$originalExe),@("CLEAN_B_BINARY",$cleanExe))) {
    if(Test-Path $pair[1]) {
        Note "BINARY_$($pair[0])_SHA256=$((Get-FileHash $pair[1] -Algorithm SHA256).Hash)"
    } else {
        Note "BINARY_$($pair[0])=MISSING"
    }
}
Note "QT_ROOT=$QtRoot"
Note "REPOSITORY_CWD=$root"
Note "GATE_TEST_CWD=$(Join-Path $OriginalBuild 'tests')"
Note "CLEAN_B_TEST_CWD=$(Join-Path $CleanCandidateBuild 'tests')"
Run "C0_gate_direct_root" $originalExe $root $false
Run "C1_gate_direct_testdir" $originalExe (Join-Path $OriginalBuild "tests") $false
Run "C2_gate_cmake_wrapper_testdir" $originalExe (Join-Path $OriginalBuild "tests") $true
Run "C3_clean_direct_root" $cleanExe $root $false
Run "C4_clean_direct_testdir" $cleanExe (Join-Path $CleanCandidateBuild "tests") $false
Run "C5_clean_cmake_wrapper_testdir" $cleanExe (Join-Path $CleanCandidateBuild "tests") $true
Note "CONTEXT_PROBE_COMPLETE=YES"
# No diagnostic can change the genuine CTest result.
exit 0
