# Diagnostic-only exact Qt launcher. Invoked from CMake execute_process.
# This introduces a PowerShell parent without changing QTest argv or CWD.
param(
    [Parameter(Mandatory=$true)][string]$QtExe,
    [Parameter(Mandatory=$true)][string]$QtLog
)
$ErrorActionPreference = 'Continue'
& $QtExe -v2 -o "$QtLog,txt"
exit $LASTEXITCODE
