# Native Windows QtTest launcher for the existing CTest diagnostic wrapper.
# The exact 16-case Qt smoke has been observed to fail when directly spawned
# by CMake execute_process EVEN on immutable FQ99 (CI #31), while the SAME
# executable, same cwd and args PASS with CMake -> pwsh -> Qt.
# Do not change QTest arguments, environment, Qt assertions, or mask exit code.
param(
    [Parameter(Mandatory=$true)][string]$QtExe,
    [Parameter(Mandatory=$true)][string]$QtLog
)
$ErrorActionPreference = 'Continue'
& $QtExe -v2 -o "$QtLog,txt"
exit $LASTEXITCODE
