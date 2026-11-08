[CmdletBinding()]
param(
    [string]$Executable = '',
    [string]$PythonExecutable = '',
    [string]$QtPrefix = '',
    [switch]$Wait,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$AppArguments = @()
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if (-not $Executable) {
    foreach ($candidate in @('out/visual-studio/desktop/RelWithDebInfo/breakwater.exe', 'out/desktop/desktop/breakwater.exe')) {
        $path = Join-Path $projectRoot $candidate
        if (Test-Path -LiteralPath $path -PathType Leaf) { $Executable = $path; break }
    }
}
if (-not $Executable -or -not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw 'Build the desktop first, or provide -Executable with the installed breakwater.exe path.'
}
$Executable = (Resolve-Path -LiteralPath $Executable).Path
if (-not $PythonExecutable) { $PythonExecutable = [Environment]::GetEnvironmentVariable('BREAKWATER_PYTHON', 'Process') }
if (-not $PythonExecutable) {
    $localPython = Join-Path $projectRoot '.tools/science-cpu/Scripts/python.exe'
    if (Test-Path -LiteralPath $localPython -PathType Leaf) { $PythonExecutable = $localPython }
}
if ($PythonExecutable -and ($PythonExecutable.Contains('\') -or $PythonExecutable.Contains('/'))) {
    $PythonExecutable = [IO.Path]::GetFullPath($PythonExecutable)
}
$executableDirectory = Split-Path -Parent $Executable
if (-not $QtPrefix -and -not (Test-Path -LiteralPath (Join-Path $executableDirectory 'Qt6Core.dll'))) {
    $localQt = Join-Path $projectRoot '.tools/Qt/6.12.0/msvc2022_64'
    if (Test-Path -LiteralPath (Join-Path $localQt 'bin') -PathType Container) { $QtPrefix = $localQt }
}
$start = [Diagnostics.ProcessStartInfo]::new()
$start.FileName = $Executable
$start.WorkingDirectory = $executableDirectory
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
if ($start.PSObject.Properties['ArgumentList']) {
    foreach ($argument in $AppArguments) { [void]$start.ArgumentList.Add($argument) }
} else {
    $start.Arguments = (@($AppArguments | ForEach-Object {
        $escaped = [regex]::Replace($_, '(\\*)"', '$1$1\"')
        '"' + [regex]::Replace($escaped, '(\\+)$', '$1$1') + '"'
    }) -join ' ')
}
if ($PythonExecutable) { $start.EnvironmentVariables['BREAKWATER_PYTHON'] = $PythonExecutable }
$start.EnvironmentVariables['PYTHONDONTWRITEBYTECODE'] = '1'
if ($QtPrefix) {
    $qtBin = Join-Path ([IO.Path]::GetFullPath($QtPrefix)) 'bin'
    if (-not (Test-Path -LiteralPath $qtBin -PathType Container)) { throw 'QtPrefix must contain the Qt bin directory.' }
    $start.EnvironmentVariables['PATH'] = "$qtBin;$($start.EnvironmentVariables['PATH'])"
}
# Hosted planning is optional. Preserve process configuration or use the locally
# configured user key without printing it, writing it, or requiring it for science.
if (-not $start.EnvironmentVariables['NVIDIA_API_KEY']) {
    $localKey = [Environment]::GetEnvironmentVariable('NVIDIA_API_KEY', 'User')
    if ($localKey) { $start.EnvironmentVariables['NVIDIA_API_KEY'] = $localKey }
}
$process = [Diagnostics.Process]::Start($start)
if (-not $process) { throw 'The desktop could not be started.' }
if ($Wait) {
    try {
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "Desktop exited with code $($process.ExitCode)." }
    } finally { $process.Dispose() }
} else {
    Write-Host "BREAKWATER started (process $($process.Id))."
    $process.Dispose()
}
