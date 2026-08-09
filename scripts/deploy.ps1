#!/usr/bin/env pwsh
#Requires -Version 5.1
# Deploy the built RedEclipseHeadTracking.asi to the game's bin/amd64/
# directory for local testing, installing the ASI loader alongside it if the
# game does not already have one.
#
# Usage: deploy.ps1 [Debug|Release] [GamePath]
# Defaults to Debug. An explicit GamePath wins over auto-detection
# (same contract as install.cmd).

param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$GamePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir  = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $scriptDir

$asi = Join-Path $projectDir "bin/$Configuration/RedEclipseHeadTracking.asi"
if (-not (Test-Path $asi)) {
    throw "Build output not found: $asi. Run 'pixi run build' or 'pixi run build-release' first."
}

if ($GamePath) {
    if (-not (Test-Path $GamePath)) {
        throw "Explicit game path does not exist: $GamePath"
    }
    $gamePath = $GamePath
} else {
    Import-Module (Join-Path $projectDir 'cameraunlock-core/powershell/GamePathDetection.psm1') -Force
    $gamePath = Find-GamePath -GameId 'red-eclipse'
    if (-not $gamePath) {
        throw "Could not locate Red Eclipse. Set RED_ECLIPSE_PATH, install via Steam, or pass the game path: deploy.ps1 $Configuration <path>"
    }
}

$exeDir = Join-Path $gamePath 'bin\amd64'
if (-not (Test-Path $exeDir)) {
    throw "Expected exe directory not found: $exeDir"
}

# redeclipse.exe imports winmm.dll, so that is the proxy slot the ASI loader
# takes over. Left alone if something already occupies it.
$loaderTarget = Join-Path $exeDir 'winmm.dll'
if (-not (Test-Path $loaderTarget)) {
    $vendorLoader = Join-Path $projectDir 'vendor\ultimate-asi-loader\dinput8.dll'
    if (-not (Test-Path $vendorLoader)) {
        throw "Vendored ASI loader not found: $vendorLoader. Run 'pixi run update-deps' first."
    }
    Copy-Item $vendorLoader -Destination $loaderTarget -Force
    Write-Host "Installed ASI loader as winmm.dll" -ForegroundColor Green
}

Copy-Item $asi -Destination $exeDir -Force
Write-Host "Deployed: $asi -> $exeDir" -ForegroundColor Green
Write-Host ""
Write-Host "Controls:" -ForegroundColor Cyan
Write-Host "  Home      - Recenter head tracking       (Ctrl+Shift+T)"
Write-Host "  End       - Toggle head tracking on/off  (Ctrl+Shift+Y)"
Write-Host "  Page Up   - Cycle tracking mode          (Ctrl+Shift+G)"
Write-Host "  Page Down - Toggle yaw mode              (Ctrl+Shift+H)"
