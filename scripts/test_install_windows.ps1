<# Exercise the real Inno Setup installer with synthetic payloads on a clean,
   disposable GitHub-hosted Windows runner. Never launch the fixture binaries. #>
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -ne 'true' -or $env:RUNNER_ENVIRONMENT -ne 'github-hosted' -or $env:RUNNER_OS -ne 'Windows') {
    throw 'This machine-wide installer fixture requires a disposable GitHub-hosted Windows runner.'
}
$root = Split-Path -Parent $PSScriptRoot
$work = Join-Path $env:RUNNER_TEMP ('luthier-install-check-' + [Guid]::NewGuid())
$content = Join-Path $env:ProgramData 'Luthier/Resources/factory-fixture.json'
$policy = Join-Path $env:ProgramData 'Luthier/luthier-policy.json'
$policySource = Join-Path $work 'machine-policy-fixture.json'
$common = [Environment]::GetFolderPath('CommonProgramFiles')
$apps = @{}
$setups = @{}
foreach ($slug in 'Pro', 'Free') {
    $apps[$slug] = Join-Path $work "apps/Luthier $slug"
    foreach ($path in "HKLM:/Software/Luthier/$slug", (Join-Path $common "VST3/Luthier $slug.vst3"),
                     (Join-Path $common "CLAP/Luthier $slug.clap"), (Join-Path $env:ProgramFiles "Luthier $slug")) {
        if (Test-Path $path) { throw "Refusing to touch an existing installation: $path" }
    }
}
if (Test-Path (Join-Path $env:ProgramData 'Luthier')) { throw 'Shared Luthier data already exists.' }
New-Item -ItemType Directory -Path $work | Out-Null
Set-Content $policySource '{"fixture":"administrator policy"}'
$policyHash = (Get-FileHash $policySource).Hash

function Invoke-Checked([string] $File, [string[]] $Arguments) {
    $process = Start-Process -FilePath $File -ArgumentList $Arguments -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "$File exited $($process.ExitCode)." }
}
function Assert-File([string] $Path, [bool] $Exists) {
    if ((Test-Path -LiteralPath $Path) -ne $Exists) { throw "Unexpected file presence ($Exists): $Path" }
}
function Install-Fixture([string] $Slug, [bool] $WithPolicy = $false) {
    $arguments = @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/SP-',
        "/DIR=`"$($apps[$Slug])`"", "/LOG=`"$(Join-Path $work "install-$Slug.log")`"")
    if ($WithPolicy) { $arguments += "/POLICY=`"$policySource`"" }
    Invoke-Checked $setups[$Slug] $arguments
}
function Uninstall-Fixture([string] $Slug) {
    $uninstaller = Join-Path $apps[$Slug] 'unins000.exe'
    if (Test-Path $uninstaller) {
        Invoke-Checked $uninstaller @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART',
            "/LOG=`"$(Join-Path $work "uninstall-$Slug.log")`"")
    }
}
try {
    $iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty Source
    if (-not $iscc) {
        $iscc = Get-ChildItem "${env:ProgramFiles(x86)}/Inno Setup */ISCC.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $iscc) {
        choco install innosetup -y --no-progress | Out-Host
        if ($LASTEXITCODE -ne 0) { throw 'Inno Setup installation failed.' }
        $iscc = Get-ChildItem "${env:ProgramFiles(x86)}/Inno Setup */ISCC.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
        if (-not $iscc) { throw 'ISCC.exe was not found after installation.' }
    }
    foreach ($slug in 'Pro', 'Free') {
        $stage = Join-Path $work "stage-$slug"
        $vst = Join-Path $stage "Luthier $slug.vst3/Contents/x86_64-win"
        New-Item -ItemType Directory -Force -Path $vst, (Join-Path $stage 'Resources') | Out-Null
        Set-Content (Join-Path $vst "Luthier $slug.vst3") 'synthetic fixture; never execute'
        Set-Content (Join-Path $stage "Luthier $slug.exe") 'synthetic fixture; never execute'
        Set-Content (Join-Path $stage "Luthier $slug.clap") 'synthetic fixture; never execute'
        Set-Content (Join-Path $stage 'Resources/factory-fixture.json') '{"shared":true}'
        Copy-Item (Join-Path $root 'Resources/luthier.ico') $stage
        $output = Join-Path $work "setup-$slug"
        New-Item -ItemType Directory -Path $output | Out-Null
        & $iscc '/Qp' '/DAppVersion=0.1.0-beta2' "/DEditionSlug=$slug" "/DStageDir=$stage" "/DOutputDir=$output" (Join-Path $root 'packaging/windows/Luthier.iss')
        if ($LASTEXITCODE -ne 0) { throw "ISCC failed for $slug." }
        $setups[$slug] = Join-Path $output "Luthier-$slug-0.1.0-beta2-Setup-win64.exe"
    }
    foreach ($first in 'Pro', 'Free') {
        $second = if ($first -eq 'Pro') { 'Free' } else { 'Pro' }
        Write-Host "Install $first and $second; reinstall $first; remove $first then $second"
        Install-Fixture $first $true
        Install-Fixture $second
        Install-Fixture $first $true
        $before = (Get-FileHash $content).Hash
        Uninstall-Fixture $first
        Assert-File $content $true
        Assert-File $policy $true
        if ((Get-FileHash $policy).Hash -ne $policyHash) { throw 'Machine policy changed during edition removal.' }
        if ((Get-FileHash $content).Hash -ne $before) { throw 'Shared factory content changed during removal.' }
        Assert-File (Join-Path $apps[$second] "Luthier $second.exe") $true
        Assert-File (Join-Path $common "VST3/Luthier $second.vst3/Contents/x86_64-win/Luthier $second.vst3") $true
        Assert-File (Join-Path $common "CLAP/Luthier $second.clap") $true
        Uninstall-Fixture $second
        Assert-File $content $false
        Assert-File $policy $true
        if ((Get-FileHash $policy).Hash -ne $policyHash) { throw 'Final removal changed administrator policy.' }
        Remove-Item -LiteralPath $policy
        Write-Host "PASS: $first removal preserved $second; final removal cleaned factory content and retained administrator policy."
    }
} finally {
    foreach ($slug in 'Pro', 'Free') { Uninstall-Fixture $slug }
    if ((Test-Path $policy) -and (Get-FileHash $policy).Hash -eq $policyHash) {
        Remove-Item -LiteralPath $policy
    }
    # Retain fixture logs until the runner is discarded, including on failure.
    Write-Host "Installer fixture logs: $work"
}
