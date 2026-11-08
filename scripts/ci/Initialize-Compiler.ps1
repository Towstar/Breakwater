# Initialize VS 2022 without forwarding an unbounded inherited PATH to cmd.exe.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$locator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $locator)) { throw 'Visual Studio locator is unavailable.' }
$installation = & $locator -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'A Visual Studio 2022 x64 C++ toolchain is required.' }

# VsDevCmd uses cmd.exe, whose command/variable expansion is limited to 8191
# characters. Resolve only the tools used by these checks before invoking it.
# No user/system PATH or registry value is changed.
$directories = [Collections.Generic.List[string]]::new()
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$bundledTools = @{
    'cmake.exe' = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
    'ninja.exe' = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
    'pwsh.exe' = Join-Path $PSHOME 'pwsh.exe'
}
foreach ($name in @('cmake.exe', 'ninja.exe', 'git.exe', 'pwsh.exe', 'python.exe')) {
    $tool = Get-Command $name -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    $toolPath = if ($tool) { $tool.Source } elseif ($bundledTools.ContainsKey($name)) { $bundledTools[$name] } else { $null }
    if (-not $toolPath -or -not (Test-Path -LiteralPath $toolPath -PathType Leaf)) {
        if ($name -eq 'python.exe') { continue }
        throw "A required CI tool is unavailable before compiler setup: $name"
    }
    $directory = [IO.Path]::GetDirectoryName($toolPath)
    if ($seen.Add($directory)) { $directories.Add($directory) }
}
foreach ($directory in @($PSHOME, (Join-Path $env:SystemRoot 'System32'), $env:SystemRoot, (Join-Path $env:SystemRoot 'System32/Wbem'))) {
    if ($seen.Add($directory)) { $directories.Add($directory) }
}
$bootstrapPath = $directories -join ';'
if ($bootstrapPath.Length -gt 6000) { throw "Resolved CI bootstrap PATH is too long ($($bootstrapPath.Length) characters)." }
$previousPath = $env:PATH
try {
    $env:PATH = $bootstrapPath
    & (Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Host
    $compiler = (Get-Command cl.exe -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
    [pscustomobject]@{
        compiler = [Diagnostics.FileVersionInfo]::GetVersionInfo($compiler).ProductVersion
        visual_studio = (& $locator -latest -products '*' -version '[17.0,18.0)' -property installationVersion).Trim()
        bootstrap_path_length = $bootstrapPath.Length
    }
} catch {
    $env:PATH = $previousPath
    throw
}
