[CmdletBinding()]
param([string]$QtPrefix = '', [string]$PythonExecutable = '', [switch]$Science,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$AppArguments = @())
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $projectRoot 'out/portfolio-desktop/desktop/breakwater.exe'
if ($Science) {
    $PythonExecutable = Join-Path $projectRoot '.tools/science-physicsnemo/Scripts/python.exe'
    if (-not (Test-Path -LiteralPath $PythonExecutable)) { throw 'Run Setup-OptionalScience.ps1 first.' }
}
& (Join-Path $PSScriptRoot 'Start-Desktop.ps1') -Executable $executable -QtPrefix $QtPrefix -PythonExecutable $PythonExecutable -AppArguments $AppArguments
