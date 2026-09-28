<#
.SYNOPSIS
    Package the Windows products staged by scripts/ci_build.ps1.

.DESCRIPTION
    Produces dist\installers\Luthier-<version>-Setup-win64.exe with Inno Setup
    (packaging\windows\Luthier.iss), and a portable zip containing the full
    VST3 bundle, standalone executable and factory Resources:
    dist\installers\Luthier-<version>-portable-win64.zip.
    -PortableOnly skips Inno Setup and writes a SHA-256 sidecar.

    Code signing runs only when WINDOWS_CERT_PFX_BASE64 (a base64 .pfx) and
    WINDOWS_CERT_PASSWORD are set: the plug-in binaries and the standalone are
    signed before packing, then the installer and its uninstaller. Without them
    the output is unsigned and a warning says so.

    Note: since June 2023 new OV/EV code-signing certificates live on hardware
    tokens or cloud HSMs and cannot be exported as a .pfx. For those, replace
    Invoke-Sign below with the provider's signer (Azure Trusted Signing's
    "Azure/trusted-signing-action", DigiCert KeyLocker's smctl, ...) - see
    docs/RELEASING.md.

.PARAMETER PortableOnly
    Package staged products without locating, installing or invoking Inno Setup.

.PARAMETER Version
    Defaults to $env:VERSION, then to project(VERSION) in CMakeLists.txt.
#>

[CmdletBinding()]
param(
    [string] $Version = $env:VERSION,
    [string] $DistDir = 'dist',
    [switch] $PortableOnly
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

# Require the complete Windows VST3 bundle before signing or packaging.
foreach ($required in 'Luthier.exe', 'Resources', 'Luthier.vst3/Contents/x86_64-win/Luthier.vst3') {
    if (-not (Test-Path (Join-Path $stage $required))) {
        throw "Staged Windows product missing: $required (run scripts/ci_build.ps1 -Step stage)."
    }
}

function Write-Step([string] $message) {
    Write-Host ''
    Write-Host "==> $message" -ForegroundColor Cyan
}

#------------------------------------------------------------------ signing
$signing = $false
$signingTemp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$pfx = Join-Path $signingTemp 'luthier-signing.pfx'
$timestamp = 'http://timestamp.digicert.com'

$signtool = $null
if ($env:WINDOWS_CERT_PFX_BASE64 -and $env:WINDOWS_CERT_PASSWORD) {
    [System.IO.File]::WriteAllBytes($pfx, [System.Convert]::FromBase64String($env:WINDOWS_CERT_PFX_BASE64))
    $signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" |
                Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $signtool) { throw 'signtool.exe not found (Windows SDK).' }
    $signing = $true
} else {
    Write-Host '::warning::WINDOWS_CERT_PFX_BASE64 / WINDOWS_CERT_PASSWORD not set: the Windows package is unsigned'
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

    if (-not $PortableOnly) {
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
    }

    #-------------------------------------------------------------- portable zip
    # installer.md 9: unpacks anywhere; no registry, no start menu. The content
    # sits in Resources beside the exe, where IrLibrary finds it first.
    Write-Step 'Building the portable zip'
    $portable = Join-Path ([System.IO.Path]::GetTempPath()) ("Luthier-$Version-portable-" + [System.IO.Path]::GetRandomFileName())
    New-Item -ItemType Directory -Path $portable | Out-Null
    try {
    Copy-Item (Join-Path $stage 'Luthier.exe') $portable
    Copy-Item -Recurse (Join-Path $stage 'Resources') (Join-Path $portable 'Resources')
    Copy-Item -Recurse (Join-Path $stage 'Luthier.vst3') $portable
    if (Test-Path (Join-Path $stage 'Luthier.clap')) { Copy-Item (Join-Path $stage 'Luthier.clap') $portable }
    Set-Content -Path (Join-Path $portable 'README.txt') -Value @"
Luthier $Version - portable

Run Luthier.exe with Resources beside it. This archive does not install
registry entries or Start menu shortcuts. For VST3, copy the entire
Luthier.vst3 folder into C:\Program Files\Common Files\VST3\, and copy
Resources into C:\ProgramData\Luthier\Resources\ for factory content.
If included, Luthier.clap belongs in C:\Program Files\Common Files\CLAP\.
Close your DAW before replacing the plug-in and rescan after copying.
"@
    $zip = Join-Path $out "Luthier-$Version-portable-win64.zip"
    if (Test-Path $zip) { Remove-Item -Force $zip }
    if (Test-Path "$zip.sha256") { Remove-Item -Force "$zip.sha256" }
    Compress-Archive -Path "$portable\*" -DestinationPath $zip
    } finally {
        Remove-Item -Recurse -Force $portable
    }
    $checksum = (Get-FileHash -Algorithm SHA256 -Path $zip).Hash.ToLowerInvariant()
    Set-Content -Path "$zip.sha256" -Encoding ascii -Value "$checksum  $(Split-Path -Leaf $zip)"

    Get-ChildItem $out | Format-Table Name, Length
}
finally {
    if (Test-Path $pfx) { Remove-Item -Force $pfx }
}
