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

function Copy-LicenseBundle {
    param(
        [Parameter(Mandatory)][string]$StagingDir,
        [Parameter(Mandatory)][string]$ProjectDir
    )
    $licenseDir = Join-Path $StagingDir 'licenses'
    New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
    $bundle = @{
        'cameraunlock-core-LICENSE.txt' = Join-Path $ProjectDir 'cameraunlock-core/LICENSE'
        'minhook-LICENSE.txt'           = Join-Path $ProjectDir 'extern/minhook/LICENSE.txt'
    }
    foreach ($name in $bundle.Keys) {
        $src = $bundle[$name]
        if (-not (Test-Path $src)) {
            throw "Licence for a component compiled into the mod is missing: $src"
        }
        Copy-Item $src -Destination (Join-Path $licenseDir $name) -Force
        Write-Host "  licenses/$name" -ForegroundColor Green
    }
}

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
# The loader binary is redistributed here, so its MIT notice travels with it.
# A Test-Path guard would turn that compliance failure into a green build.
foreach ($vendorFile in @('dinput8.dll', 'LICENSE', 'README.md')) {
    $src = Join-Path $vendorAsiDir $vendorFile
    if (-not (Test-Path $src)) {
        throw "Vendored loader file missing: vendor/ultimate-asi-loader/$vendorFile"
    }
    Copy-Item $src -Destination $ghVendorDir -Force
}
Write-Host '  vendor/ultimate-asi-loader/dinput8.dll + LICENSE' -ForegroundColor Green

foreach ($doc in @('README.md', 'LICENSE', 'CHANGELOG.md', 'THIRD-PARTY-NOTICES.md')) {
    $p = Join-Path $projectDir $doc
    if (-not (Test-Path $p)) {
        throw "Required document not found: $doc. Every published ZIP is a binary distribution and must carry its notices."
    }
    Copy-Item -Path $p -Destination $ghStaging -Force
}

# MinHook (BSD-2-Clause) and cameraunlock-core (MIT, a different copyright
# holder from this mod's own LICENSE) are both compiled into the .asi, so their
# notices have to accompany the binary in every ZIP that carries it.
Copy-LicenseBundle -StagingDir $ghStaging -ProjectDir $projectDir

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
# The Nexus ZIP is a binary distribution too: the licences of everything
# compiled into or bundled with the payload require their notices to travel
# with it, so LICENSE and THIRD-PARTY-NOTICES.md ship at its root.
foreach ($noticeDoc in @('LICENSE', 'THIRD-PARTY-NOTICES.md', 'README.md')) {
    $noticeSrc = Join-Path $projectDir $noticeDoc
    if (-not (Test-Path $noticeSrc)) {
        throw "Required notice file not found: $noticeDoc. Every published ZIP is a binary distribution and must carry it."
    }
    Copy-Item $noticeSrc -Destination $nexusStaging -Force
    Write-Host "  $noticeDoc" -ForegroundColor Green
}
Copy-LicenseBundle -StagingDir $nexusStaging -ProjectDir $projectDir
Push-Location $nexusStaging
try { Compress-Archive -Path '.\*' -DestinationPath $nexusZip -Force } finally { Pop-Location }
Remove-Item -Recurse -Force $nexusStaging

$nexusKb = [math]::Round((Get-Item $nexusZip).Length / 1KB, 1)
Write-Host ("  $nexusZip ({0:N1} KB)" -f $nexusKb) -ForegroundColor Green

Write-Host ''
Write-Host '=== Package Complete ===' -ForegroundColor Magenta

Write-Output $installerZip
Write-Output $nexusZip
