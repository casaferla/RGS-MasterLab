# Local-only runner for the genuine Product benchmark; no upload or copying of WAVs.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceWav,
    [Parameter(Mandatory=$true)][string]$ReferenceWav,
    [Parameter(Mandatory=$true)][ValidateSet(44100,48000,88200,96000,176400,192000)][int]$SampleRate,
    [string]$TestExe=(Join-Path $PSScriptRoot 'rgsml_project_session_tests.exe'),
    [string]$QtBin=(Join-Path $PSScriptRoot '../bin'),
    [Parameter(Mandatory=$true)][string]$ReportDirectory
)
$ErrorActionPreference='Stop'
$src=(Resolve-Path -LiteralPath $SourceWav).Path
$ref=(Resolve-Path -LiteralPath $ReferenceWav).Path
$exe=(Resolve-Path -LiteralPath $TestExe).Path
$qt=(Resolve-Path -LiteralPath $QtBin).Path
if ($src -eq $ref) { throw 'Use two independently owned WAV files.' }
foreach ($wav in @($src,$ref)) {
    if (!(Test-Path -LiteralPath $wav -PathType Leaf) -or [IO.Path]::GetExtension($wav) -ne '.wav') {
        throw 'Expected a local WAV file.'
    }
}
if ([IO.Path]::GetFileName($exe) -ne 'rgsml_project_session_tests.exe') { throw 'Expected the actual Product session test executable.' }
if (!(Test-Path -LiteralPath (Join-Path $qt 'Qt6Core.dll'))) { throw 'Qt runtime not found.' }
$reportRoot=New-Item -ItemType Directory -Force -Path $ReportDirectory
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$report=Join-Path $reportRoot.FullName "L3-music-$SampleRate-$stamp.txt"
$keys=@('PATH','RGSML_L3_E2E_RATE','RGSML_L3_BENCH_SOURCE_WAV','RGSML_L3_BENCH_REFERENCE_WAV','QTEST_FUNCTION_TIMEOUT','RGSML_PLAYBACK_FIR_ROOT')
$previous=@{}
foreach ($key in $keys) { $previous[$key]=[Environment]::GetEnvironmentVariable($key,'Process') }
try {
    $env:PATH="$(Split-Path -Parent $exe);$qt;$env:PATH"
    $fir=Join-Path $qt 'resources/fir'
    if (Test-Path -LiteralPath $fir -PathType Container) { $env:RGSML_PLAYBACK_FIR_ROOT=$fir }
    $env:RGSML_L3_E2E_RATE=[string]$SampleRate
    $env:RGSML_L3_BENCH_SOURCE_WAV=$src
    $env:RGSML_L3_BENCH_REFERENCE_WAV=$ref
    $env:QTEST_FUNCTION_TIMEOUT='4200000'
    $elapsed=[Diagnostics.Stopwatch]::StartNew()
    & $exe nativeMeasuredWinnerUsesRealQtM12AndM13 -o -,txt -o "$report,txt"
    $code=$LASTEXITCODE
    $elapsed.Stop()
    @("exit_code=$code","elapsed_wall_seconds=$($elapsed.Elapsed.TotalSeconds)","sample_rate_hz=$SampleRate","test_executable_sha256=$((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash)","computer=$env:COMPUTERNAME") |
        Set-Content -LiteralPath "$report.run.txt" -Encoding utf8
    if ($code -ne 0) { throw "Native Product benchmark failed; see $report" }
    if (!(Test-Path -LiteralPath $report -PathType Leaf)) { throw 'Native QtTest report missing.' }
    Write-Host "Local report: $report"
} finally {
    foreach ($key in $keys) { [Environment]::SetEnvironmentVariable($key,$previous[$key],'Process') }
}
