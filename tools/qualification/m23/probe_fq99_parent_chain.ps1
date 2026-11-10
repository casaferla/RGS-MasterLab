# M23 causal diagnosis: qualified FQ99 vs candidate in a SAME-runner
# Windows parent-chain test. NEVER changes released QML / test assertions.
# Each arm is the FULL 16-case Qt smoke, screenshots expressly OFF.
param(
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$CandidateBuild,
    [Parameter(Mandatory=$true)][string]$BoundedReport
)
$ErrorActionPreference="Continue"
$baseSha="5e4871d5849e4121b0612c2142d1e051d6c9a3d2"
$candidateSha="ba8bf9ea41b4d4e950bb780f035a368f1f1a10c9"
$src=Join-Path $env:RUNNER_TEMP "rgsml-parent-fq99-src"
$build=Join-Path $env:RUNNER_TEMP "rgsml-parent-fq99-build"
$tmp=Join-Path $env:RUNNER_TEMP "rgsml-parent-chain-logs"
New-Item -ItemType Directory -Path $tmp -Force | Out-Null
$cmakeDriver=Join-Path $env:GITHUB_WORKSPACE "tools/qualification/m23/probe_qt_parent_chain.cmake"
$psChild=Join-Path $env:GITHUB_WORKSPACE "tools/qualification/m23/probe_qt_powershell_child.ps1"
Set-Content -Path $BoundedReport -Value @(
    "M23_PARENT_CHAIN=DIAGNOSTIC_ONLY",
    "BASELINE_SHA=$baseSha",
    "CANDIDATE_SOURCE_SHA=$candidateSha",
    "DIAGNOSTIC_RUN_SHA=$env:GITHUB_SHA",
    "SCREENSHOT_ENV=EXPLICITLY_OFF",
    "QPA_AND_BACKEND=DEFAULT_NATIVE",
    "QTEST_SUITE=FULL_SAME_ARGUMENTS",
    "NO_FQ_OR_RELEASE_CLAIM=YES"
)
function Note([string]$x) {Add-Content -Path $BoundedReport -Value $x}
function Clip([string]$x) {
    $x=$x -replace '[\r\n]+',' '
    if($x.Length -gt 245){return $x.Substring(0,245)}
    return $x
}
$env:PATH="$QtRoot/bin;$env:PATH"
$env:QML2_IMPORT_PATH="$QtRoot/qml"
Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue
Note "QT_ROOT=$QtRoot"
function BuildBaseline() {
    $log=Join-Path $tmp "baseline-setup.log"
    & git worktree add --detach $src $baseSha *> $log
    if($LASTEXITCODE -ne 0) {Note "BASELINE_BUILD=INCONCLUSIVE_WORKTREE";return ""}
    $actual=(& git -C $src rev-parse HEAD)
    Note "BASELINE_CHECKOUT_ACTUAL=$actual"
    if($actual -ne $baseSha) {Note "BASELINE_BUILD=INCONCLUSIVE_WRONG_SHA";return ""}
    & cmake -S $src -B $build -G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Release -DRGSML_BUILD_TESTS=ON -DRGSML_WARNINGS_AS_ERRORS=ON "-DRGSML_QT_ROOT=$QtRoot" *> $log
    if($LASTEXITCODE -ne 0) {Note "BASELINE_BUILD=INCONCLUSIVE_CONFIGURE";Get-Content $log -Tail 2 | ForEach-Object {Note ("BUILD_ERROR="+(Clip $_))};return ""}
    & cmake --build $build --config Release --parallel 4 --target rgsml_source_metadata_panel_smoke *> $log
    if($LASTEXITCODE -ne 0) {Note "BASELINE_BUILD=INCONCLUSIVE_COMPILATION";Get-Content $log -Tail 2 | ForEach-Object {Note ("BUILD_ERROR="+(Clip $_))};return ""}
    $exe=Join-Path $build "Release/rgsml_source_metadata_panel_smoke.exe"
    if(!(Test-Path $exe)){Note "BASELINE_BUILD=INCONCLUSIVE_NO_EXE";return ""}
    Note "BASELINE_BUILD=SUCCESS"
    return $exe
}
function RunArm([string]$tag,[string]$exe,[string]$cwd,[string]$mode) {
    if(!(Test-Path $exe) -or !(Test-Path $cwd)){
        Note "ARM=$tag STATUS=INCONCLUSIVE_MISSING_EXE_OR_CWD"
        return
    }
    $log=Join-Path $tmp "$tag-qt.txt"
    $outer=Join-Path $tmp "$tag-parent.txt"
    Remove-Item $log,$outer -ErrorAction SilentlyContinue
    Remove-Item Env:RGSML_GUI01_EVIDENCE_DIR -ErrorAction SilentlyContinue
    Push-Location $cwd
    try {
        $start=Get-Date
        if($mode -eq "DIRECT") {
            & $exe -v2 -o "$log,txt" *> $outer
            $parentExit=$LASTEXITCODE
            $qtExit="$parentExit"
        } else {
            & cmake "-DRGSML_GUI_EXE=$exe" "-DRGSML_GUI_LOG=$log" "-DRGSML_PARENT_MODE=$mode" "-DRGSML_PWSH_LAUNCHER=$psChild" -P $cmakeDriver *> $outer
            $parentExit=$LASTEXITCODE
            $marker=@(Select-String -Path $outer -Pattern 'M23_QTEST_EXIT=' -ErrorAction SilentlyContinue)
            $qtExit=if($marker.Count){($marker[-1].Line -split "M23_QTEST_EXIT=",2)[1].Trim()}else{"NO_MARKER"}
        }
        $elapsed=[math]::Round(((Get-Date)-$start).TotalSeconds,1)
    } catch {
        $parentExit=-999;$qtExit="EXCEPTION";$elapsed=-1
        Note ("ARM_EXCEPTION_$tag="+(Clip $_.Exception.Message))
    } finally {
        Pop-Location
    }
    $fails=if(Test-Path $log){@(Select-String -Path $log -Pattern 'FAIL!|QFATAL|FATAL' | Select-Object -First 2)}else{@()}
    $summary=if(Test-Path $log){@(Select-String -Path $log -Pattern '^Totals: ' | Select-Object -Last 1)}else{@()}
    $status=if(!(Test-Path $log) -or "$qtExit" -in @("NO_MARKER","EXCEPTION")) {"INCONCLUSIVE"}
            elseif("$qtExit" -eq "0" -and $fails.Count -eq 0) {"PASS"} else {"FAIL"}
    Note "ARM=$tag MODE=$mode STATUS=$status QT_EXIT=$qtExit PARENT_EXIT=$parentExit TIME_S=$elapsed"
    if($summary.Count){Note ("TOTAL_$tag="+(Clip $summary[0].Line))}
    foreach($f in $fails){Note ("FIRST_FAIL_$tag="+(Clip $f.Line))}
    if($status -eq "INCONCLUSIVE" -and (Test-Path $outer)){
        Get-Content $outer -Tail 3 | ForEach-Object {Note ("OUTER_$tag="+(Clip $_))}
    }
}
$candExe=Join-Path $CandidateBuild "Release/rgsml_source_metadata_panel_smoke.exe"
$candCwd=Join-Path $CandidateBuild "tests"
if(Test-Path $candExe) {Note "CANDIDATE_EXE_SHA256=$((Get-FileHash $candExe -Algorithm SHA256).Hash)"}
RunArm "G0_candidate_direct" $candExe $candCwd "DIRECT"
RunArm "G1_candidate_cmake_direct" $candExe $candCwd "CMAKE_DIRECT"
RunArm "G2_candidate_cmake_pwsh_child" $candExe $candCwd "CMAKE_PWSH_CHILD"
$baseExe=BuildBaseline
if($baseExe){
    Note "BASELINE_EXE_SHA256=$((Get-FileHash $baseExe -Algorithm SHA256).Hash)"
    $baseCwd=Join-Path $build "tests"
    RunArm "A0_fq99_direct" $baseExe $baseCwd "DIRECT"
    RunArm "A1_fq99_cmake_direct" $baseExe $baseCwd "CMAKE_DIRECT"
    RunArm "A2_fq99_cmake_pwsh_child" $baseExe $baseCwd "CMAKE_PWSH_CHILD"
} else {Note "BASELINE_ARMS=INCONCLUSIVE"}
RunArm "G3_candidate_cmake_direct_repeat" $candExe $candCwd "CMAKE_DIRECT"
RunArm "G4_candidate_direct_repeat" $candExe $candCwd "DIRECT"
Note "PARENT_CHAIN_COMPLETE=YES"
# Diagnostics never change authoritative real CTest failure state.
exit 0
