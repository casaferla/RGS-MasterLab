# RGS SkinPack R1 — isolated Windows Qt 6.8+ build and startup probe.
# Must be run in a checkout of the experimental branch, on a Windows PC with
# Visual Studio 2022, CMake, Qt 6.8.3 MSVC2022_64 + Qt Multimedia.
# This does NOT dispatch GitHub Actions and does NOT change production M16.
param(
    [string]$QtRoot = $env:RGSML_QT_ROOT,
    [switch]$SkipStartup
)
$ErrorActionPreference = "Stop"
$expected = "experimental/skinpack-r1-r4-unqualified-20261016"
$branch = (& git branch --show-current).Trim()
if ($branch -ne $expected) { throw "Wrong branch: $branch. Expected $expected." }
if (-not $QtRoot -or -not (Test-Path (Join-Path $QtRoot "lib\cmake\Qt6\Qt6Config.cmake"))) {
    throw "Set RGSML_QT_ROOT to an installed Qt 6.8.3 MSVC2022_64 directory."
}
$build = Join-Path $PWD "build\skinpack-r1-probe"
$stage = Join-Path $build "stage"
& cmake -S . -B $build -G "Visual Studio 17 2022" -A x64 `
    "-DRGSML_QT_ROOT=$QtRoot" "-DRGSML_BUILD_TESTS=OFF" "-DRGSML_WARNINGS_AS_ERRORS=ON"
if ($LASTEXITCODE -ne 0) { throw "Qt configure failed." }
& cmake --build $build --config Release --target rgsml_app --parallel 2
if ($LASTEXITCODE -ne 0) { throw "Qt app build failed." }
if (-not $SkipStartup) {
    $env:PATH = "$(Join-Path $QtRoot 'bin');$env:PATH"
    $env:QML2_IMPORT_PATH = Join-Path $QtRoot "qml"
    & cmake --install $build --config Release --prefix $stage
    if ($LASTEXITCODE -ne 0) { throw "Qt deploy/install failed." }
    $exe = Join-Path $stage "bin\RGSMasterLab.exe"
    if (-not (Test-Path $exe)) { throw "Staged executable missing: $exe" }
    # Windows GUI-subsystem executables can return control to PowerShell
    # before their GUI event loop exits. Start-Process -Wait is mandatory.
    $deployProcess = Start-Process -FilePath $exe -ArgumentList "--rgsml-deploy-smoke" -NoNewWindow -PassThru -Wait
    if ($deployProcess.ExitCode -ne 0) { throw "Qt/QML startup smoke failed: $($deployProcess.ExitCode)" }
    # Synthetic ZIP STORE fixture: real native minizip + Qt PNG decode, QML
    # visibility, rejected-corrupt-ZIP rollback, and RGS Original reset.
    # Synthetic fixture is not the real Kintsugi artwork or a pixel qualification.
    $fixtureDir = Join-Path $build "skinpack-fixtures"
    & python ./tools/skinpack/generate_r1_smoke_fixture.py $fixtureDir
    if ($LASTEXITCODE -ne 0) { throw "SkinPack fixture generation failed." }
    $validFixture = Join-Path $fixtureDir "r1-valid.rgsskin"
    $syntheticProcess = Start-Process -FilePath $exe -ArgumentList ('--rgsml-skinpack-smoke="' + $validFixture + '"') -NoNewWindow -PassThru -Wait
    if ($syntheticProcess.ExitCode -ne 0) { throw "SkinPack native import/rollback/reset smoke failed: $($syntheticProcess.ExitCode)" }

    # Exact artwork uploaded by PO to isolated branch, never fetched from an
    # unpinned URL or altered by fixture generation.
    $artFile = Join-Path $PWD "skinpack\Kintsugi_Prestige_R1_ArtFamily_v0.3_STORE_UNQUALIFIED.rgsskin"
    if (-not (Test-Path -LiteralPath $artFile)) {
        throw "Missing real Kintsugi R1 v0.3 art archive in isolated checkout"
    }
    $artSize = (Get-Item -LiteralPath $artFile).Length
    if ($artSize -ne 9394629) { throw "Art package size mismatch: $artSize" }
    $artSha = (Get-FileHash -LiteralPath $artFile -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($artSha -ne "61186747376723442adaa18c3ba4d81e1939eed0475b565c49c48374bcd4b55a") {
        throw "Art package SHA-256 mismatch: $artSha"
    }
    Write-Host "SKINPACK_R1_REAL_ART_ARCHIVE_EXACT_SHA256_PASS"
    $artProcess = Start-Process -FilePath $exe -ArgumentList ('--rgsml-skinpack-art-smoke="' + $artFile + '"') -NoNewWindow -PassThru -Wait
    if ($artProcess.ExitCode -ne 0) { throw "Real Kintsugi R1 v0.3 Qt import/screenshot/reset smoke failed: $($artProcess.ExitCode)" }
    foreach ($resolution in @("1184", "expanded")) {
        $screenshot = Join-Path $build ("skinpack-art-" + $resolution + ".png")
        if (-not (Test-Path -LiteralPath $screenshot)) {
            throw "Real Kintsugi art screenshot missing: $screenshot"
        }
        & python ./tools/skinpack/ci/emit_art_screenshot.py $screenshot $resolution
        if ($LASTEXITCODE -ne 0) { throw "Cannot verify screenshot evidence for $resolution" }
    }
}
Write-Host "EXPERIMENTAL_R1_QT_BUILD_AND_REAL_KINTSUGI_ART_SMOKE_PASS (not pixel/audio/full qualification)"
