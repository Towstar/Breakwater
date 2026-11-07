param(
    [ValidateSet('headless','desktop','asan')][string]$Preset = 'headless',
    [string]$QtPrefix = '',
    [switch]$NoTests,
    [switch]$EmbeddedDebugInfo
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$locatorPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    if (-not (Test-Path -LiteralPath $locatorPath)) { throw 'Install Visual Studio C++ tools, or run in its developer shell.' }
    $installation = & $locatorPath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installation) { throw 'Visual Studio x64 C++ tools were not found.' }
    & "$installation\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
}
if (-not $QtPrefix -and (Test-Path -LiteralPath "$projectRoot\.tools\Qt\6.12.0\msvc2022_64")) {
    $QtPrefix = "$projectRoot\.tools\Qt\6.12.0\msvc2022_64"
}
$configureArguments = @('--preset', $Preset)
if ($EmbeddedDebugInfo) { $configureArguments += '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded' }
if ($QtPrefix) { $configureArguments += "-DCMAKE_PREFIX_PATH=$QtPrefix" }
Push-Location -LiteralPath $projectRoot
try {
    & cmake @configureArguments
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
    if (-not $NoTests) {
        & ctest --preset $Preset
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    }
} finally { Pop-Location }
