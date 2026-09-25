<#
.SYNOPSIS
    Build the Windows installer from the products scripts/ci_build.ps1 staged.

.DESCRIPTION
    Produces dist\installers\Luthier-<version>-Setup-win64.exe with Inno Setup
    (packaging\windows\Luthier.iss), and a portable zip of the standalone
    (installer.md 9): dist\installers\Luthier-<version>-portable-win64.zip.

    Code signing runs only when WINDOWS_CERT_PFX_BASE64 (a base64 .pfx) and
    WINDOWS_CERT_PASSWORD are set: the plug-in binaries and the standalone are
    signed before packing, then the installer and its uninstaller. Without them
    the output is unsigned and a warning says so.

    Note: since June 2023 new OV/EV code-signing certificates live on hardware
    tokens or cloud HSMs and cannot be exported as a .pfx. For those, replace
    Invoke-Sign below with the provider's signer (Azure Trusted Signing's
    "Azure/trusted-signing-action", DigiCert KeyLocker's smctl, ...) - see
    docs/RELEASING.md.

.PARAMETER Version
    Defaults to $env:VERSION, then to project(VERSION) in CMakeLists.txt.
#>

[CmdletBinding()]
param(
    [string] $Version = $env:VERSION,
    [string] $DistDir = 'dist'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $Version) {
    $m = Select-String -Path 'CMakeLists.txt' -Pattern '^project\(Luthier VERSION ([0-9.]+)'
    $Version = $m.Matches[0].Groups[1].Value
}

$stage = (Resolve-Path (Join-Path $DistDir 'windows')).Path
$out   = Join-Path $root "$DistDir/installers"
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Write-Step([string] $message) {
    Write-Host ''
    Write-Host "==> $message" -ForegroundColor Cyan
}

#------------------------------------------------------------------ signing
$signing = $false
$pfx = Join-Path $env:RUNNER_TEMP 'luthier-signing.pfx'
if (-not $env:RUNNER_TEMP) { $pfx = Join-Path ([System.IO.Path]::GetTempPath()) 'luthier-signing.pfx' }
$timestamp = 'http://timestamp.digicert.com'

$signtool = $null
if ($env:WINDOWS_CERT_PFX_BASE64 -and $env:WINDOWS_CERT_PASSWORD) {
    [System.IO.File]::WriteAllBytes($pfx, [System.Convert]::FromBase64String($env:WINDOWS_CERT_PFX_BASE64))
    $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" |
                Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $signtool) { throw 'signtool.exe not found (Windows SDK).' }
    $signing = $true
} else {
    Write-Host '::warning::WINDOWS_CERT_PFX_BASE64 / WINDOWS_CERT_PASSWORD not set: the installer is unsigned'
}

function Invoke-Sign([string] $file) {
    if (-not $signing) { return }
    & $signtool sign /f $pfx /p $env:WINDOWS_CERT_PASSWORD /fd sha256 /tr $timestamp /td sha256 `
        /d 'Luthier' $file
    if ($LASTEXITCODE -ne 0) { throw "Signing $file failed." }
}

try {
    #-------------------------------------------------------------- binaries
    if ($signing) {
        Write-Step 'Signing binaries'
        Get-ChildItem -Recurse -Path (Join-Path $stage 'Luthier.vst3') -Filter '*.vst3' -File | ForEach-Object { Invoke-Sign $_.FullName }
        foreach ($f in 'Luthier.clap', 'Luthier.exe', 'luthier-render.exe') {
            $p = Join-Path $stage $f
            if (Test-Path $p) { Invoke-Sign $p }
        }
    }

    #-------------------------------------------------------------- Inno Setup
    $iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
    if (-not $iscc) {
        foreach ($c in "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe") {
            if (Test-Path $c) { $iscc = $c; break }
        }
    }
    if (-not $iscc) {
        Write-Step 'Installing Inno Setup'
        choco install innosetup -y --no-progress | Out-Host
        $iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    }

    Write-Step "Building the installer ($Version)"
    $isccArgs = @("/DAppVersion=$Version", "/DStageDir=$stage", "/DOutputDir=$out")
    if ($signing) {
        $isccArgs += '/DSign'
        $isccArgs += "/Ssigntool=`"$signtool`" sign /f `"$pfx`" /p `"$($env:WINDOWS_CERT_PASSWORD)`" /fd sha256 /tr $timestamp /td sha256 /d Luthier `$f"
    }
    $isccArgs += (Join-Path $root 'packaging/windows/Luthier.iss')
    & $iscc @isccArgs
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)." }

    #-------------------------------------------------------------- portable zip
    # installer.md 9: unpacks anywhere; no registry, no start menu. The content
    # sits in Resources beside the exe, where IrLibrary finds it first.
    Write-Step 'Building the portable zip'
    $portable = Join-Path ([System.IO.Path]::GetTempPath()) "Luthier-$Version-portable"
    if (Test-Path $portable) { Remove-Item -Recurse -Force $portable }
    New-Item -ItemType Directory -Path $portable | Out-Null
    Copy-Item (Join-Path $stage 'Luthier.exe') $portable
    Copy-Item -Recurse (Join-Path $stage 'Resources') (Join-Path $portable 'Resources')
    Copy-Item -Recurse (Join-Path $stage 'Luthier.vst3') $portable
    if (Test-Path (Join-Path $stage 'Luthier.clap')) { Copy-Item (Join-Path $stage 'Luthier.clap') $portable }
    Set-Content -Path (Join-Path $portable 'README.txt') -Value @"
Luthier $Version - portable

Run Luthier.exe from this folder. Nothing is written to the registry or the
Start menu. To use the plug-ins, point your DAW's VST3 or CLAP folder list at
this folder, or copy Luthier.vst3 / Luthier.clap to
C:\Program Files\Common Files\VST3 and ...\CLAP (keep Resources beside them).
"@
    $zip = Join-Path $out "Luthier-$Version-portable-win64.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    Compress-Archive -Path "$portable\*" -DestinationPath $zip
    Remove-Item -Recurse -Force $portable

    Get-ChildItem $out | Format-Table Name, Length
}
finally {
    if (Test-Path $pfx) { Remove-Item -Force $pfx }
}
