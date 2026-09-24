<#
.SYNOPSIS
    Configure, build, test, validate and stage Luthier on Windows (MSVC).

.DESCRIPTION
    The Windows counterpart of scripts/ci_build.sh. The GitHub Actions workflows
    call it one step at a time; it runs the same way on a developer machine from
    a "Developer PowerShell for VS 2022" (Ninja needs the MSVC environment).

    Memory: the JUCE module translation units are large. CMakeLists.txt already
    compiles with /MP1 /bigobj for the 4 GB development machine; here the number
    of parallel Ninja jobs is what bounds memory. GitHub's Windows runners have
    16 GB and 4 cores, so the default of 4 fits; pass -Jobs 2 on a 4 GB machine
    (the same default scripts/build.ps1 uses).

.PARAMETER Step
    deps, configure, build, test, validate, stage, or all (the default).

.EXAMPLE
    scripts/ci_build.ps1
    scripts/ci_build.ps1 -Step configure,build -Jobs 2
    scripts/ci_build.ps1 -Step validate -Strictness 10
#>

[CmdletBinding()]
param(
    [ValidateSet('deps', 'configure', 'build', 'test', 'validate', 'stage', 'all')]
    [string[]] $Step = @('all'),

    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [ValidateSet('ON', 'OFF')]
    [string] $Lto = 'OFF',

    [ValidateRange(1, 64)]
    [int] $Jobs = 4,

    [ValidateRange(1, 10)]
    [int] $Strictness = 5,

    [string] $BuildDir = 'build-ci',
    [string] $DistDir = 'dist',
    [string] $LogDir = '',
    [string] $PluginvalVersion = 'v1.0.4',
    [string] $ClapValidatorVersion = '0.3.2',
    [switch] $SkipValidators
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $LogDir) { $LogDir = Join-Path $BuildDir 'logs' }
$toolsDir  = Join-Path $BuildDir 'tools'
$artefacts = Join-Path $BuildDir "Luthier_artefacts/$Config"

$JuceTag = '8.0.10'
# Keep in step with CLAP_JUCE_EXT_COMMIT in scripts/ci_build.sh.
$ClapJuceExtCommit = '55525c9858d4b25687be7759a5e0f70eccef218e'

function Write-Step([string] $message) {
    Write-Host ''
    Write-Host "==> $message" -ForegroundColor Cyan
}

function Invoke-Checked([string] $what, [scriptblock] $block) {
    & $block
    if ($LASTEXITCODE -ne 0) { throw "$what failed ($LASTEXITCODE)." }
}

#------------------------------------------------------------------------------
function Step-Deps {
    Write-Step "Fetching JUCE $JuceTag and clap-juce-extensions"
    New-Item -ItemType Directory -Force -Path 'ThirdParty' | Out-Null
    if (-not (Test-Path 'ThirdParty/JUCE/CMakeLists.txt')) {
        Invoke-Checked 'JUCE clone' { git clone --depth 1 --branch $JuceTag https://github.com/juce-framework/JUCE.git ThirdParty/JUCE }
    }
    if (-not (Test-Path 'ThirdParty/clap-juce-extensions/CMakeLists.txt')) {
        Invoke-Checked 'clap-juce-extensions clone' { git clone https://github.com/free-audio/clap-juce-extensions.git ThirdParty/clap-juce-extensions }
        Invoke-Checked 'clap-juce-extensions checkout' { git -C ThirdParty/clap-juce-extensions checkout -q $ClapJuceExtCommit }
        Invoke-Checked 'clap submodules' { git -C ThirdParty/clap-juce-extensions submodule update --init --recursive --depth 1 }
    }
}

function Step-Configure {
    Write-Step "Configuring $BuildDir ($Config, LTO $Lto)"
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        throw 'cl.exe is not on PATH. Run from a Developer PowerShell for VS 2022 (CI uses ilammy/msvc-dev-cmd).'
    }
    $cmakeArgs = @('-S', '.', '-B', $BuildDir, '-G', 'Ninja',
                   "-DCMAKE_BUILD_TYPE=$Config", "-DLUTHIER_LTO=$Lto",
                   '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl')
    if (Get-Command sccache -ErrorAction SilentlyContinue) {
        # sccache caches MSVC objects only with embedded debug info (/Z7), never
        # a shared PDB (/Zi); CMP0141 lets CMake choose that for us.
        $cmakeArgs += @('-DCMAKE_C_COMPILER_LAUNCHER=sccache', '-DCMAKE_CXX_COMPILER_LAUNCHER=sccache',
                        '-DCMAKE_POLICY_DEFAULT_CMP0141=NEW',
                        '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=$<$<CONFIG:Debug>:Embedded>')
    }
    Invoke-Checked 'CMake configure' { cmake @cmakeArgs }
}

function Step-Build {
    $targets = @('Luthier_VST3', 'Luthier_Standalone', 'LuthierTests', 'LuthierRender')
    if (Test-Path 'ThirdParty/clap-juce-extensions') { $targets += 'Luthier_CLAP' }
    Write-Step "Building $($targets -join ' ') with $Jobs jobs"
    Invoke-Checked 'Build' { cmake --build $BuildDir --config $Config --parallel $Jobs --target @targets }
}

function Step-Test {
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
    $runner = Join-Path $BuildDir "LuthierTests_artefacts/$Config/LuthierTests.exe"
    if (-not (Test-Path $runner)) { throw "Test runner not found at $runner." }
    Write-Step 'Running the unit tests'
    & $runner 2>&1 | Tee-Object -FilePath (Join-Path $LogDir 'unit-tests.log')
    if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)." }
}

function Get-Pluginval {
    $exe = Join-Path $toolsDir 'pluginval.exe'
    if (-not (Test-Path $exe)) {
        New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
        $zip = Join-Path $toolsDir 'pluginval_Windows.zip'
        Invoke-WebRequest -UseBasicParsing -OutFile $zip `
            "https://github.com/Tracktion/pluginval/releases/download/$PluginvalVersion/pluginval_Windows.zip"
        Expand-Archive -Force $zip $toolsDir
    }
    return $exe
}

function Get-ClapValidator {
    $exe = Join-Path $toolsDir 'clap-validator.exe'
    if (-not (Test-Path $exe)) {
        New-Item -ItemType Directory -Force -Path $toolsDir | Out-Null
        $name = "clap-validator-$ClapValidatorVersion-windows.zip"
        $zip = Join-Path $toolsDir $name
        Invoke-WebRequest -UseBasicParsing -OutFile $zip `
            "https://github.com/free-audio/clap-validator/releases/download/$ClapValidatorVersion/$name"
        Expand-Archive -Force $zip $toolsDir
        if (-not (Test-Path $exe)) {
            $found = Get-ChildItem -Recurse -Filter 'clap-validator.exe' $toolsDir | Select-Object -First 1
            if ($found) { Copy-Item $found.FullName $exe }
        }
    }
    return $exe
}

function Step-Validate {
    if ($SkipValidators) { Write-Step 'Skipping validators'; return }
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
    $failed = $false

    $pluginval = Get-Pluginval
    $vst3 = Join-Path $artefacts 'VST3/Luthier.vst3'
    Write-Step "pluginval strictness ${Strictness}: $vst3"
    & $pluginval --strictness-level $Strictness --validate-in-process --timeout-ms 600000 `
        --output-dir $LogDir --validate $vst3 2>&1 |
        Tee-Object -FilePath (Join-Path $LogDir 'pluginval-Luthier.vst3.log')
    if ($LASTEXITCODE -ne 0) { $failed = $true }

    $clap = Join-Path $artefacts 'CLAP/Luthier.clap'
    if (Test-Path $clap) {
        $validator = Get-ClapValidator
        Write-Step "clap-validator: $clap"
        & $validator validate $clap 2>&1 | Tee-Object -FilePath (Join-Path $LogDir 'clap-validator.log')
        if ($LASTEXITCODE -ne 0) { $failed = $true }
    }

    if ($failed) { throw 'Plugin validation failed; see the logs above.' }
}

function Step-Stage {
    $out = Join-Path $DistDir 'windows'
    Write-Step "Staging products into $out"
    if (Test-Path $out) { Remove-Item -Recurse -Force $out }
    New-Item -ItemType Directory -Force -Path $out | Out-Null

    Copy-Item -Recurse (Join-Path $artefacts 'VST3/Luthier.vst3') $out
    $clap = Join-Path $artefacts 'CLAP/Luthier.clap'
    if (Test-Path $clap) { Copy-Item $clap $out }
    Copy-Item (Join-Path $artefacts 'Standalone/Luthier.exe') $out
    # The console app's file is named after its target; it ships as luthier-render.exe.
    $render = Join-Path $BuildDir "LuthierRender_artefacts/$Config/LuthierRender.exe"
    if (Test-Path $render) { Copy-Item $render (Join-Path $out 'luthier-render.exe') }

    # One shared copy of the factory content (installer.md 1.1: ProgramData).
    Copy-Item -Recurse 'Resources' (Join-Path $out 'Resources')
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $out 'Resources/icon*.png')
    Copy-Item 'Resources/luthier.ico' $out

    # The VST3 bundle keeps JUCE's moduleinfo.json but not our content.
    foreach ($d in 'BodyIRs', 'CabIRs', 'Fonts', 'Guitars', 'Parts', 'Presets', 'Tunes', 'icon.png', 'icon_small.png', 'luthier.ico') {
        $p = Join-Path $out "Luthier.vst3/Contents/Resources/$d"
        if (Test-Path $p) { Remove-Item -Recurse -Force $p }
    }
    Set-Content -Path (Join-Path $out 'BUILD_CONFIG') -Value $Config
}

#------------------------------------------------------------------------------
foreach ($s in $Step) {
    switch ($s) {
        'deps'      { Step-Deps }
        'configure' { Step-Configure }
        'build'     { Step-Build }
        'test'      { Step-Test }
        'validate'  { Step-Validate }
        'stage'     { Step-Stage }
        'all'       { Step-Deps; Step-Configure; Step-Build; Step-Test; Step-Validate; Step-Stage }
    }
}

Write-Step 'Done'
