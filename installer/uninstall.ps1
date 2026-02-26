<#
.SYNOPSIS
    Uninstall the PhoneVR-MotoG5 SteamVR driver and remove firewall rules.
.NOTES
    SPDX-License-Identifier: GPL-3.0-only
    PhoneVR-MotoG5 — installer/uninstall.ps1
    Copyright (C) 2024 PhoneVR-MotoG5 contributors
    Run as Administrator.
#>
#Requires -RunAsAdministrator

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$DriverName    = "phonevr_motog5"
$DiscoveryPort = 33333
$ControlPort   = 33334
$PosePort      = 33335

function Write-Step([string]$msg) {
    Write-Host "`n==> $msg" -ForegroundColor Cyan
}
function Write-OK([string]$msg) {
    Write-Host "    [OK] $msg" -ForegroundColor Green
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
    Write-Host "    Steam not found in registry — nothing to remove" -ForegroundColor Yellow
} else {
    $SteamVRPath = Join-Path $SteamPath "steamapps\common\SteamVR"
    $DriverDest  = Join-Path $SteamVRPath "drivers\$DriverName"

    # ── 2. Remove driver folder ────────────────────────────────────────────────
    Write-Step "Removing driver from SteamVR/drivers/$DriverName"
    if (Test-Path $DriverDest) {
        Remove-Item -Recurse -Force $DriverDest
        Write-OK "Removed $DriverDest"
    } else {
        Write-Host "    Driver directory not found — already removed or never installed" -ForegroundColor Yellow
    }

    # ── 3. Revert steamvr.vrsettings ──────────────────────────────────────────
    Write-Step "Reverting activateMultipleDrivers in steamvr.vrsettings"
    $SteamVRSettings = "$env:LOCALAPPDATA\openvr\steamvr.vrsettings"
    if (Test-Path $SteamVRSettings) {
        $settings = Get-Content $SteamVRSettings -Raw | ConvertFrom-Json
        if ($settings.PSObject.Properties.Name -contains "steamvr") {
            $steamvr = $settings.steamvr
            if ($steamvr.PSObject.Properties.Name -contains "activateMultipleDrivers") {
                $steamvr.PSObject.Properties.Remove("activateMultipleDrivers")
                $settings | ConvertTo-Json -Depth 10 | Set-Content $SteamVRSettings -Encoding UTF8
                Write-OK "activateMultipleDrivers removed from steamvr.vrsettings"
            } else {
                Write-OK "activateMultipleDrivers was not set — nothing to revert"
            }
        }
    } else {
        Write-Host "    steamvr.vrsettings not found — skipping" -ForegroundColor Yellow
    }
}

# ── 4. Remove firewall rules ──────────────────────────────────────────────────
Write-Step "Removing Windows Firewall rules"

$ruleNames = @(
    "PhoneVR Discovery UDP In",
    "PhoneVR Control TCP In",
    "PhoneVR Pose UDP In"
)

foreach ($name in $ruleNames) {
    $rule = Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue
    if ($rule) {
        Remove-NetFirewallRule -DisplayName $name
        Write-OK "Removed rule '$name'"
    } else {
        Write-Host "    Rule '$name' not found — already removed" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Uninstallation complete." -ForegroundColor Green
Write-Host "Please restart SteamVR."
