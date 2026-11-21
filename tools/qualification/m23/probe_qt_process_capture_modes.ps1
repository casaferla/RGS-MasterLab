# M23 Windows Qt GUI full-suite launch-path experiment, DIAGNOSTIC ONLY.
# One exact binary, same CWD, Qt 6.8.3 native backend, screenshots OFF.
# Compare direct launch to CMake execute_process with output piping modes.
param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$OriginalBuild,
    [Parameter(Mandatory=$true)][string]$BoundedReport
)
$ErrorActionPreference="Continue"
$exe=Join-Path $OriginalBuild "Release/rgsml_source_metadata_panel_smoke.exe"
$cwd=Join-Path $OriginalBuild "tests"
$driver=Join-Path $env:GITHUB_WORKSPACE "tools/qualification/m23/probe_qt_cmake_capture_mode.cmake"
$temp=Join-Path $env:RUNNER_TEMP "m23-qt-capture-mode-local"
New-Item -ItemType Directory -Path $temp -Force | Out-Null
Set-Content -Path $BoundedReport -Value @(
    "RGSML_M23_CAPTURE_PATH_DIAGNOSTIC_ONLY=YES",
    "TEST_SOURCE_SHA=ba8bf9ea41b4d4e950bb780f035a368f1f1a10c9",
    "GITHUB_RUN_SHA=$env:GITHUB_SHA",
    "EXPLICIT_SCREENSHOTS=OFF",
    "CMAKE_DRIVER=probe_qt_cmake_capture_mode.cmake",
    "UNMODIFIED_TEST_GEOMETRY_ASSERTION=YES",
    "NO_PRODUCT_CHANGE=YES"
)
function Note([string]$msg) {Add-Content -Path $BoundedReport -Value $msg}
function Clip([string]$x) {
    $x = $x -replace '[\r\n]+',' '
    if($x.Length -gt 250) {return $x.Substring(0,250)}
    return $x
}
if(!(Test-Path $exe) -or !(Test-Path $cwd) -or !(Test-Path $driver)) {
    Note "DIAGNOSTIC=INCONCLUSIVE_MISSING_INPUT"
    exit 0
}
Note "EXE_SHA256=$((Get-FileHash $exe -Algorithm SHA256).Hash)"
Note "EXE=$exe"
Note "CWD=$cwd"
Note "QT_ROOT=$QtRoot"
$env:PATH="$QtRoot/bin;$env:PATH"
$env:QML2_IMPORT_PATH="$QtRoot/qml"
Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue

function RunArm([string]$tag,[string]$mode) {
    $qtlog=Join-Path $temp "$tag-qt.txt"
    $outer=Join-Path $temp "$tag-parent-output.txt"
    Remove-Item $qtlog,$outer -ErrorAction SilentlyContinue
    Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
    Push-Location $cwd
    $started=Get-Date
    try {
        if($mode -eq "DIRECT") {
            & $exe -v2 -o "$qtlog,txt" *> $outer
            $parentExit=$LASTEXITCODE
            $qtExit=$parentExit
        } else {
            & cmake "-DRGSML_TEST_EXE=$exe" "-DRGSML_QT_LOG=$qtlog" "-DRGSML_CAPTURE_MODE=$mode" -P $driver *> $outer
            $parentExit=$LASTEXITCODE
            $markers=@(Select-String -Path $outer -Pattern 'M23_QT_PROCESS_EXIT=' -ErrorAction SilentlyContinue)
            $qtExit = if($markers.Count) {
                ($markers[-1].Line -split "M23_QT_PROCESS_EXIT=",2)[1].Trim()
            } else {"MISSING"}
        }
    } catch {
        $parentExit=-998
        $qtExit="EXCEPTION"
        Note ("ERROR_$tag="+(Clip $_.Exception.Message))
    } finally {
        Pop-Location
    }
    $elapsed=[math]::Round(((Get-Date)-$started).TotalSeconds,1)
    $fail=if(Test-Path $qtlog) {@(Select-String -Path $qtlog -Pattern 'FAIL!|QFATAL|FATAL' | Select-Object -First 2)}else{@()}
    $totals=if(Test-Path $qtlog) {@(Select-String -Path $qtlog -Pattern '^Totals: ' | Select-Object -Last 1)}else{@()}
    $status = if(!(Test-Path $qtlog) -or $qtExit -in @("MISSING","EXCEPTION")) {"INCONCLUSIVE"}
        elseif("$qtExit" -eq "0" -and $fail.Count -eq 0) {"PASS"} else {"FAIL"}
    Note "ARM=$tag MODE=$mode STATUS=$status QT_EXIT=$qtExit PARENT_EXIT=$parentExit ELAPSED_S=$elapsed"
    if($totals.Count) {Note ("TOTAL_$tag="+(Clip $totals[0].Line))}
    foreach($line in $fail) {Note ("FIRST_FAILURE_$tag="+(Clip $line.Line))}
    if($status -eq "INCONCLUSIVE" -and (Test-Path $outer)) {
        Get-Content $outer -Tail 3 | ForEach-Object {Note ("OUTER_$tag="+(Clip $_))}
    }
}
# Direct before/after confirms repeatability. Same full QTest suite in each.
RunArm "P0_direct" "DIRECT"
RunArm "P1_cmake_no_capture" "NO_CAPTURE"
RunArm "P2_cmake_variables" "CAPTURE_VARIABLES"
RunArm "P3_cmake_files" "CAPTURE_FILES"
RunArm "P4_cmake_stdout_only" "CAPTURE_STDOUT_ONLY"
RunArm "P5_cmake_no_capture_repeat" "NO_CAPTURE"
RunArm "P6_cmake_variables_repeat" "CAPTURE_VARIABLES"
RunArm "P7_direct_repeat" "DIRECT"
Note "CAPTURE_MODE_MATRIX_COMPLETE=YES"
# All results are experimental. The original M23 CTest gate remains fatal.
exit 0
