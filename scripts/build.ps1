<#
.SYNOPSIS
    Configure, build and test Luthier.

.DESCRIPTION
    A thin wrapper over the CMake commands in README.md. It exists because this
    project is routinely built on a 4 GB machine, where the JUCE module
    translation units will exhaust memory if MSVC is left to pick its own
    parallelism. The default of two jobs is what fits.

.PARAMETER Target
    Which target to build. "All" builds the plugin, the standalone, the test
    runner and the offline renderer.

.PARAMETER Config
    Release (default) or Debug.

.PARAMETER Jobs
    Parallel compile jobs. Two is the safe default here; raise it on a machine
    with more memory.

.PARAMETER Test
    Run the test suite after a successful build. Implied by -Target Tests.

.PARAMETER Clean
    Delete the build directory and reconfigure from scratch.

.EXAMPLE
    scripts/build.ps1
    scripts/build.ps1 -Target Tests
    scripts/build.ps1 -Target VST3 -Config Debug
    scripts/build.ps1 -Clean -Jobs 4 -Test
#>

[CmdletBinding()]
param(
    [ValidateSet('All', 'VST3', 'Standalone', 'Tests', 'Render')]
    [string] $Target = 'All',

    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',

    [ValidateRange(1, 64)]
    [int] $Jobs = 2,

    [switch] $Test,
    [switch] $Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Repository root, regardless of where this was invoked from.
$root  = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'

function Write-Step([string] $message) {
    Write-Host ''
    Write-Host "==> $message" -ForegroundColor Cyan
}

#------------------------------------------------------------------ preflight
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake was not found on PATH. Install CMake 3.22 or newer.'
}

$juce = Join-Path $root 'ThirdParty/JUCE'
if (-not (Test-Path (Join-Path $juce 'CMakeLists.txt'))) {
    throw "JUCE is missing from $juce. Fetch it first - see the Build section of README.md."
}

#------------------------------------------------------------------ configure
if ($Clean -and (Test-Path $build)) {
    Write-Step "Removing $build"
    Remove-Item -Recurse -Force $build
}

if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) {
    Write-Step 'Configuring'
    cmake -B $build -S $root -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)." }
}

#---------------------------------------------------------------------- build
# Luthier_All is the JUCE-generated aggregate: every plugin format at once.
$targets = switch ($Target) {
    'All'        { @('Luthier_All', 'LuthierTests', 'LuthierRender') }
    'VST3'       { @('Luthier_VST3') }
    'Standalone' { @('Luthier_Standalone') }
    'Tests'      { @('LuthierTests') }
    'Render'     { @('LuthierRender') }
}

foreach ($t in $targets) {
    Write-Step "Building $t ($Config, $Jobs jobs)"
    cmake --build $build --config $Config --target $t --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "Build of $t failed ($LASTEXITCODE)." }
}

#----------------------------------------------------------------------- test
if ($Test -or $Target -eq 'Tests') {
    $runner = Join-Path $build "LuthierTests_artefacts/$Config/LuthierTests.exe"
    if (-not (Test-Path $runner)) {
        throw "Test runner not found at $runner."
    }

    Write-Step 'Running the test suite'
    & $runner
    if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)." }
}

Write-Step 'Done'
