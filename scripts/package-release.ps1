#!/usr/bin/env pwsh
#Requires -Version 5.1
# Custom packaging for Red Eclipse Head Tracking (C++ project, no .csproj).
# Produces two ZIPs:
#   - RedEclipseHeadTracking-v{version}-installer.zip (GitHub Release)
#   - RedEclipseHeadTracking-v{version}-nexus.zip     (Nexus, extract to game folder)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectDir 'cameraunlock-core/powershell/ReleaseWorkflow.psm1') -Force

$cmakeLists = Get-Content (Join-Path $projectDir 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'project\(RedEclipseHeadTracking VERSION (\d+\.\d+\.\d+)') {
    throw "Could not parse version from CMakeLists.txt"
}
$version = $Matches[1]
$modName = 'RedEclipseHeadTracking'

Write-Host ""
Write-Host "=== Packaging $modName v$version ===" -ForegroundColor Magenta
Write-Host ""

$releaseDir = Join-Path $projectDir 'release'
if (-not (Test-Path $releaseDir)) { New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null }

$asiPath = Join-Path $projectDir "bin/Release/$modName.asi"
if (-not (Test-Path $asiPath)) {
    throw "$modName.asi not found at: $asiPath. Run 'pixi run build-release' first."
}

$vendorAsiDir = Join-Path $projectDir 'vendor/ultimate-asi-loader'
$vendorAsiDll = Join-Path $vendorAsiDir 'dinput8.dll'
if (-not (Test-Path $vendorAsiDll)) {
    throw "Bundled ASI loader missing: $vendorAsiDll. Run 'pixi run update-deps' first."
}

$scriptsDir = Join-Path $projectDir 'scripts'
foreach ($s in @('install.cmd', 'uninstall.cmd')) {
    if (-not (Test-Path (Join-Path $scriptsDir $s))) {
        throw "Required script not found: $s"
    }
}

$launcherManifestPath = Join-Path $projectDir 'launcher-manifest.json'
if (-not (Test-Path $launcherManifestPath)) {
    throw "launcher-manifest.json not found at: $launcherManifestPath"
}

# --- Installer ZIP -----------------------------------------------------
Write-Host '--- Installer ZIP ---' -ForegroundColor Yellow

$ghStaging = Join-Path $releaseDir 'staging-installer'
if (Test-Path $ghStaging) { Remove-Item -Recurse -Force $ghStaging }
New-Item -ItemType Directory -Path $ghStaging -Force | Out-Null

foreach ($s in @('install.cmd', 'uninstall.cmd')) {
    Copy-Item (Join-Path $scriptsDir $s) -Destination $ghStaging -Force
}

# install.cmd / uninstall.cmd resolve the game via shared/find-game.ps1 on
# every run, so that shim has to travel with them or the ZIP is dead on
# arrival. Copy-SharedBundle carries games.json + GamePathDetection.psm1 too.
Copy-SharedBundle -StagingDir $ghStaging

$pluginsDir = Join-Path $ghStaging 'plugins'
New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
Copy-Item $asiPath -Destination $pluginsDir -Force
Write-Host "  plugins/$modName.asi" -ForegroundColor Green

# The ASI loader ships as the raw DLL, referenced straight from
# launcher-manifest.json's files[] and renamed to winmm.dll on deploy. No
# wrapper zip: a loader.archives entry would need a zip the vendoring step
# never produces, which is how several mods shipped with a manifest pointing
# at a file that was not in the package.
$ghVendorDir = Join-Path $ghStaging 'vendor/ultimate-asi-loader'
New-Item -ItemType Directory -Path $ghVendorDir -Force | Out-Null
foreach ($vendorFile in @('dinput8.dll', 'LICENSE', 'README.md')) {
    $src = Join-Path $vendorAsiDir $vendorFile
    if (Test-Path $src) { Copy-Item $src -Destination $ghVendorDir -Force }
}
Write-Host '  vendor/ultimate-asi-loader/dinput8.dll' -ForegroundColor Green

foreach ($doc in @('README.md', 'LICENSE', 'CHANGELOG.md', 'THIRD-PARTY-NOTICES.md')) {
    $p = Join-Path $projectDir $doc
    if (Test-Path $p) { Copy-Item -Path $p -Destination $ghStaging -Force }
}

# Canonical launcher manifest the launcher ingests to deploy the package.
# Stamp mod_info.version from the build so the shipped manifest can never
# disagree with the built .asi.
$stagedManifest = Join-Path $ghStaging 'launcher-manifest.json'
$manifestText = Get-Content $launcherManifestPath -Raw
$manifestText = $manifestText -replace '("version":\s*")\d+\.\d+\.\d+(")', "`${1}$version`$2"
[System.IO.File]::WriteAllText($stagedManifest, $manifestText, (New-Object System.Text.UTF8Encoding $false))
Write-Host "  launcher-manifest.json (version $version)" -ForegroundColor Green

$installerZip = Join-Path $releaseDir "$modName-v$version-installer.zip"
if (Test-Path $installerZip) { Remove-Item $installerZip -Force }
Push-Location $ghStaging
try { Compress-Archive -Path '.\*' -DestinationPath $installerZip -Force } finally { Pop-Location }
Remove-Item -Recurse -Force $ghStaging

$installerKb = [math]::Round((Get-Item $installerZip).Length / 1KB, 1)
Write-Host ("  $installerZip ({0:N1} KB)" -f $installerKb) -ForegroundColor Green

# Every manifest source path must actually exist inside the ZIP. Catching that
# here is the difference between a failed build and a user whose launcher
# reports "Installed" over a game folder with nothing deployed.
$validator = Join-Path $projectDir 'cameraunlock-core/scripts/validate-manifest.mjs'
if (Test-Path $validator) {
    & node $validator $installerZip
    if ($LASTEXITCODE -ne 0) { throw "validate-manifest rejected $installerZip" }
    Write-Host '  manifest validated' -ForegroundColor Green
} else {
    throw "validate-manifest.mjs not found at $validator. Run 'pixi run sync' to update cameraunlock-core."
}

# --- Nexus ZIP ---------------------------------------------------------
Write-Host ''
Write-Host '--- Nexus ZIP ---' -ForegroundColor Yellow

$nexusStaging = Join-Path $releaseDir 'staging-nexus'
if (Test-Path $nexusStaging) { Remove-Item -Recurse -Force $nexusStaging }

# Nexus users manage their own ASI loader, so the nexus ZIP ships only the
# mod's .asi - never the vendored dinput8.dll.
$nexusGameDir = Join-Path $nexusStaging 'bin\amd64'
New-Item -ItemType Directory -Path $nexusGameDir -Force | Out-Null

Copy-Item $asiPath -Destination $nexusGameDir -Force

$nexusZip = Join-Path $releaseDir "$modName-v$version-nexus.zip"
if (Test-Path $nexusZip) { Remove-Item $nexusZip -Force }
Push-Location $nexusStaging
try { Compress-Archive -Path '.\*' -DestinationPath $nexusZip -Force } finally { Pop-Location }
Remove-Item -Recurse -Force $nexusStaging

$nexusKb = [math]::Round((Get-Item $nexusZip).Length / 1KB, 1)
Write-Host ("  $nexusZip ({0:N1} KB)" -f $nexusKb) -ForegroundColor Green

Write-Host ''
Write-Host '=== Package Complete ===' -ForegroundColor Magenta

Write-Output $installerZip
Write-Output $nexusZip
