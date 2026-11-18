[CmdletBinding()]
param([Parameter(Mandatory)][string]$Bundle, [Parameter(Mandatory)][string]$EvidenceDirectory)
$ErrorActionPreference = 'Stop'
$Bundle = (Resolve-Path -LiteralPath $Bundle).Path
$python = Join-Path $Bundle 'runtime/python/python.exe'
if (-not (Test-Path -LiteralPath $python)) { throw 'The bundle-owned Python runtime is missing.' }
# Source and packaged verification share one implementation. The verifier creates
# a private child environment and never logs or forwards a credential.
& $python -B -I (Join-Path $PSScriptRoot 'verify_portfolio_bundle.py') $Bundle ([IO.Path]::GetFullPath($EvidenceDirectory))
if ($LASTEXITCODE -ne 0) { throw "Portable verification failed ($LASTEXITCODE). Inspect $EvidenceDirectory." }
