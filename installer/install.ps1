<#
.SYNOPSIS
    Install the PhoneVR-MotoG5 SteamVR driver and configure firewall rules.
.DESCRIPTION
    - Detects SteamVR installation from the registry.
    - Copies the driver to SteamVR/drivers/phonevr_motog5/.
    - Enables activateMultipleDrivers in steamvr.vrsettings.
    - Adds Windows Firewall inbound rules for UDP 33333, TCP 33334, UDP 33335.
.NOTES
    SPDX-License-Identifier: GPL-3.0-only
    PhoneVR-MotoG5 — installer/install.ps1
    Copyright (C) 2024 PhoneVR-MotoG5 contributors
    Run as Administrator.
#>
#Requires -RunAsAdministrator

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# ── Configuration ─────────────────────────────────────────────────────────────
$DriverName      = "phonevr_motog5"
$DriverDLL       = "driver_phonevr_motog5.dll"
$DiscoveryPort   = 33333
$ControlPort     = 33334
$PosePort        = 33335

$ScriptDir       = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot        = Split-Path -Parent $ScriptDir
$DriverSource    = Join-Path $RepoRoot "pc-driver"

# ── Helper functions ──────────────────────────────────────────────────────────
function Write-Step([string]$msg) {
    Write-Host "`n==> $msg" -ForegroundColor Cyan
}

function Write-OK([string]$msg) {
    Write-Host "    [OK] $msg" -ForegroundColor Green
}

function Write-Fail([string]$msg) {
    Write-Host "    [FAIL] $msg" -ForegroundColor Red
    exit 1
}

# ── 1. Locate SteamVR ─────────────────────────────────────────────────────────
Write-Step "Locating SteamVR installation"

$SteamRegPaths = @(
    "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam",
    "HKLM:\SOFTWARE\Valve\Steam",
    "HKCU:\SOFTWARE\Valve\Steam"
)

$SteamPath = $null
foreach ($reg in $SteamRegPaths) {
    if (Test-Path $reg) {
        $SteamPath = (Get-ItemProperty $reg -ErrorAction SilentlyContinue).InstallPath
        if ($SteamPath -and (Test-Path $SteamPath)) { break }
    }
}

if (-not $SteamPath) {
    Write-Fail "Steam not found in registry. Install Steam/SteamVR first."
}

$SteamVRPath = Join-Path $SteamPath "steamapps\common\SteamVR"
if (-not (Test-Path $SteamVRPath)) {
    Write-Fail "SteamVR not found at '$SteamVRPath'. Install SteamVR via Steam first."
}
Write-OK "SteamVR found at: $SteamVRPath"

# ── 2. Copy driver files ──────────────────────────────────────────────────────
Write-Step "Installing driver to SteamVR/drivers/$DriverName"

$DriverDest = Join-Path $SteamVRPath "drivers\$DriverName"
if (Test-Path $DriverDest) {
    Write-Host "    Existing driver directory found — removing..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $DriverDest
}
New-Item -ItemType Directory -Path $DriverDest | Out-Null

# Copy manifest
$ManifestSrc = Join-Path $DriverSource "driver_manifest.json"
if (-not (Test-Path $ManifestSrc)) {
    Write-Fail "driver_manifest.json not found at '$ManifestSrc'. Build the driver first."
}
Copy-Item $ManifestSrc (Join-Path $DriverDest "driver.vrdrivermanifest")

# Copy resources
$ResourcesSrc = Join-Path $DriverSource "resources"
if (Test-Path $ResourcesSrc) {
    Copy-Item -Recurse $ResourcesSrc (Join-Path $DriverDest "resources")
}

# Copy DLL (from build output)
$DLLSrc = Join-Path $DriverSource "build\Release\$DriverDLL"
if (-not (Test-Path $DLLSrc)) {
    # Also try debug build
    $DLLSrc = Join-Path $DriverSource "build\Debug\$DriverDLL"
}
if (Test-Path $DLLSrc) {
    $BinDest = Join-Path $DriverDest "bin\win64"
    New-Item -ItemType Directory -Path $BinDest -Force | Out-Null
    Copy-Item $DLLSrc $BinDest
    Write-OK "Driver DLL installed to $BinDest"
} else {
    Write-Host "    [WARN] $DriverDLL not found — build the driver first (cmake --build)" -ForegroundColor Yellow
    Write-Host "           Driver manifest installed; add DLL manually later." -ForegroundColor Yellow
}

Write-OK "Driver files copied to $DriverDest"

# ── 3. Enable activateMultipleDrivers ─────────────────────────────────────────
Write-Step "Enabling activateMultipleDrivers in steamvr.vrsettings"

$VRSettingsPath = "$env:LOCALAPPDATA\openvr\openvrpaths.vrpath"
# steamvr.vrsettings is typically at:
$SteamVRSettings = "$env:LOCALAPPDATA\openvr\steamvr.vrsettings"

if (Test-Path $SteamVRSettings) {
    $settings = Get-Content $SteamVRSettings -Raw | ConvertFrom-Json
} else {
    $settings = [PSCustomObject]@{}
}

if (-not ($settings.PSObject.Properties.Name -contains "steamvr")) {
    $settings | Add-Member -MemberType NoteProperty -Name "steamvr" -Value ([PSCustomObject]@{})
}
$steamvr = $settings.steamvr
if (-not ($steamvr.PSObject.Properties.Name -contains "activateMultipleDrivers") -or
    -not $steamvr.activateMultipleDrivers) {
    $steamvr | Add-Member -MemberType NoteProperty -Name "activateMultipleDrivers" -Value $true -Force
    $settings | ConvertTo-Json -Depth 10 | Set-Content $SteamVRSettings -Encoding UTF8
    Write-OK "activateMultipleDrivers enabled"
} else {
    Write-OK "activateMultipleDrivers already enabled"
}

# ── 4. Firewall rules ─────────────────────────────────────────────────────────
Write-Step "Adding Windows Firewall rules"

$rules = @(
    @{ Name = "PhoneVR Discovery UDP In";  Protocol = "UDP"; Port = $DiscoveryPort },
    @{ Name = "PhoneVR Control TCP In";    Protocol = "TCP"; Port = $ControlPort   },
    @{ Name = "PhoneVR Pose UDP In";       Protocol = "UDP"; Port = $PosePort      }
)

foreach ($rule in $rules) {
    $existing = Get-NetFirewallRule -DisplayName $rule.Name -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host "    Rule '$($rule.Name)' already exists — skipping" -ForegroundColor Yellow
    } else {
        New-NetFirewallRule `
            -DisplayName $rule.Name `
            -Direction   Inbound `
            -Protocol    $rule.Protocol `
            -LocalPort   $rule.Port `
            -Action      Allow | Out-Null
        Write-OK "Rule '$($rule.Name)' created ($($rule.Protocol) port $($rule.Port))"
    }
}

# ── 5. Verify ─────────────────────────────────────────────────────────────────
Write-Step "Verifying installation"

$manifestDest = Join-Path $DriverDest "driver.vrdrivermanifest"
if (Test-Path $manifestDest) {
    Write-OK "driver.vrdrivermanifest present"
} else {
    Write-Fail "Manifest missing from $DriverDest"
}

Write-Host ""
Write-Host "Installation complete!" -ForegroundColor Green
Write-Host "  Driver:   $DriverDest"
Write-Host "  Ports:    UDP $DiscoveryPort (discovery), TCP $ControlPort (video), UDP $PosePort (pose)"
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1. Restart SteamVR"
Write-Host "  2. Install PhoneVR-MotoG5.apk on your phone"
Write-Host "  3. Open the app and tap 'Find PC'"
