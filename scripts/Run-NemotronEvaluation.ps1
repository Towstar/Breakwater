#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [string]$BuildDirectory = '',
    [string]$PythonExecutable = '',
    [string]$QtPrefix = '',
    [ValidatePattern('^[0-9a-f]{40}$')][string]$ExpectedSourceCommit,
    [ValidateRange(60,1800)][int]$DurationSeconds = 1800,
    [switch]$Live
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path -Parent $PSScriptRoot
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repo 'out/portfolio-desktop' }
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
if (-not $PythonExecutable) { $PythonExecutable = $env:BREAKWATER_PYTHON }
if (-not $PythonExecutable) { $PythonExecutable = Join-Path $repo '.tools/science-cpu/Scripts/python.exe' }
$PythonExecutable = (Resolve-Path -LiteralPath $PythonExecutable).Path
if (-not $QtPrefix) {
    $cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt')
    $line = @($cache | Where-Object { $_ -match '^Qt6_DIR:PATH=' })
    if ($line.Count -ne 1) { throw 'Set -QtPrefix to the configured Qt installation.' }
    $QtPrefix = [IO.Path]::GetFullPath((Join-Path ($line[0] -replace '^Qt6_DIR:PATH=', '') '../../..'))
}
$QtPrefix = (Resolve-Path -LiteralPath $QtPrefix).Path
if (-not $ExpectedSourceCommit) {
    $ExpectedSourceCommit = (& git -C $repo rev-parse HEAD).Trim()
    if ($LASTEXITCODE) { throw 'Cannot identify the source checkout.' }
}
$arguments = @('-B', (Join-Path $PSScriptRoot 'run_nemotron_evaluation.py'), 'run',
    '--output', [IO.Path]::GetFullPath($EvidenceDirectory),
    '--sandbox-executable', (Join-Path $BuildDirectory 'desktop/breakwater_live_check.exe'),
    '--eog-executable', (Join-Path $BuildDirectory 'desktop/breakwater_eog_live_check.exe'),
    '--offline-executable', (Join-Path $BuildDirectory 'desktop/breakwater_offline_check.exe'),
    '--desktop-executable', (Join-Path $BuildDirectory 'desktop/breakwater.exe'),
    '--expected-source', $ExpectedSourceCommit, '--duration-seconds', [string]$DurationSeconds)
if ($Live) { $arguments += '--live' }
# Preserve only these known process settings. Values, especially the key, are
# never logged, added to argv, written to source or persisted by this wrapper.
$names = @('PATH','BREAKWATER_PYTHON','BREAKWATER_EOG_NATIVE','BREAKWATER_EOG_PLANT',
    'BREAKWATER_EOG_WORKER','BREAKWATER_EOG_CONTRACTS','NVIDIA_API_KEY','NVIDIA_BASE_URL','NEMOTRON_MODEL')
$previous = @{}
foreach ($name in $names) { $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
try {
    $env:PATH = (Join-Path $QtPrefix 'bin') + ';' + $env:PATH
    $env:BREAKWATER_PYTHON = $PythonExecutable
    $env:BREAKWATER_EOG_NATIVE = Join-Path $BuildDirectory 'engine/breakwater_eog_response.dll'
    $env:BREAKWATER_EOG_PLANT = Join-Path $BuildDirectory 'engine/breakwater_eog_plant.dll'
    $env:BREAKWATER_EOG_WORKER = Join-Path $repo 'science/eog_worker.py'
    $env:BREAKWATER_EOG_CONTRACTS = Join-Path $repo 'contracts/eog'
    if ($Live) {
        if (-not $env:NVIDIA_API_KEY) { $env:NVIDIA_API_KEY = [Environment]::GetEnvironmentVariable('NVIDIA_API_KEY','User') }
        if (-not $env:NVIDIA_API_KEY) { throw 'Configure the local NVIDIA_API_KEY before requesting live evaluation.' }
        $env:NVIDIA_BASE_URL = 'https://integrate.api.nvidia.com/v1'
        $env:NEMOTRON_MODEL = 'nvidia/nemotron-3.5-lightning-30b-a3b'
    } else {
        Remove-Item Env:NVIDIA_API_KEY -ErrorAction SilentlyContinue
    }
    & $PythonExecutable @arguments
    if ($LASTEXITCODE -ne 0) { throw "Evaluation did not complete successfully ($LASTEXITCODE). Retain the new evidence directory." }
} finally {
    foreach ($name in $names) {
        if ($null -eq $previous[$name]) { Remove-Item -LiteralPath ('Env:' + $name) -ErrorAction SilentlyContinue }
        else { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
    }
}
