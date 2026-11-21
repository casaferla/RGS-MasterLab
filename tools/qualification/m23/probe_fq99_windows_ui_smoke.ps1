# Diagnostic only. Execute the approved FQ99 source with the failed candidate's
# Windows runner and Qt 6.8.3. The candidate's own CTest failure stays fatal.
param([Parameter(Mandatory=$true)][string]$QtRoot,
      [Parameter(Mandatory=$true)][string]$BoundedEvidence)
$ErrorActionPreference="Continue"
$baseline="5e4871d5849e4121b0612c2142d1e051d6c9a3d2"
$src=Join-Path $env:RUNNER_TEMP "rgsml-fq99-ab-source"
$build=Join-Path $env:RUNNER_TEMP "rgsml-fq99-ab-build"
$probeLog=Join-Path $env:RUNNER_TEMP "rgsml-fq99-ab-ui-log.txt"
$stdout=Join-Path $env:RUNNER_TEMP "rgsml-fq99-ab-ui-console.txt"
$setup=Join-Path $env:RUNNER_TEMP "rgsml-fq99-ab-setup.txt"
function Note([string]$s) {
    Add-Content -Path $BoundedEvidence -Value ("FQ99_AB: "+$s)
}
Note "BASELINE_SHA=$baseline"
Note "NATIVE_QT_ROOT=$QtRoot"
& git worktree add --detach $src $baseline *> $setup
if($LASTEXITCODE -ne 0){
    Note "INCONCLUSIVE_WORKTREE_FAILED"
    exit 0
}
& cmake -S $src -B $build -G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Release -DRGSML_BUILD_TESTS=ON -DRGSML_WARNINGS_AS_ERRORS=ON "-DRGSML_QT_ROOT=$QtRoot" *> $setup
if($LASTEXITCODE -ne 0) {
    Note "INCONCLUSIVE_CONFIGURE_FAILED"
    Get-Content $setup -Tail 4 | ForEach-Object { Note ("CONFIG: "+$_) }
    exit 0
}
& cmake --build $build --config Release --parallel 4 --target rgsml_source_metadata_panel_smoke *> $setup
if($LASTEXITCODE -ne 0) {
    Note "INCONCLUSIVE_BUILD_FAILED"
    Get-Content $setup -Tail 4 | ForEach-Object { Note ("BUILD: "+$_) }
    exit 0
}
$exe=Join-Path $build "Release/rgsml_source_metadata_panel_smoke.exe"
if(!(Test-Path $exe)){
    Note "INCONCLUSIVE_SMOKE_EXE_MISSING"
    exit 0
}
$env:PATH="$QtRoot/bin;$env:PATH"
$env:QML2_IMPORT_PATH="$QtRoot/qml"
Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
Remove-Item Env:QT_QUICK_BACKEND -ErrorAction SilentlyContinue
$env:RGSML_GUI01_EVIDENCE_DIR=Join-Path $env:RUNNER_TEMP "rgsml-fq99-ab-images"
New-Item -ItemType Directory -Force -Path $env:RGSML_GUI01_EVIDENCE_DIR | Out-Null
& $exe emptyReadyErrorAndWindowLifecycle -v2 -o "$probeLog,txt" *> $stdout
$code=$LASTEXITCODE
if($code -eq 0){
    Note "BASELINE_FQ99_NATIVE_SMOKE=PASS"
}else{
    Note "BASELINE_FQ99_NATIVE_SMOKE=FAIL EXIT=$code"
    if(Test-Path $probeLog){
        @(Select-String -Path $probeLog -Pattern 'FAIL!|QFATAL|Fatal error' |
          Select-Object -First 4) | ForEach-Object {
            $line=$_.Line
            if($line.Length -gt 240){$line=$line.Substring(0,240)}
            Note ("FAIL:"+$line)
        }
    }else{
        Note "NO_QT_SMOKE_LOG"
        Get-Content $stdout -Tail 3 | ForEach-Object { Note ("CONSOLE:"+$_) }
    }
}
# A passing baseline does not authorize or turn the candidate green.
exit 0
