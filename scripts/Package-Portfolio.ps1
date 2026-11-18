#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$BuildDirectory = '', [string]$ToolRoot = '',
    [Parameter(Mandatory)][string]$Destination,
    [switch]$AllowDirty
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'out/portfolio-desktop' }
if (-not $ToolRoot) { $ToolRoot = Join-Path $projectRoot '.tools' }
$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$ToolRoot = (Resolve-Path -LiteralPath $ToolRoot).Path
$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) { throw 'Choose a new destination. Packaging never replaces an existing bundle.' }
$dirty = [bool](& git -C $projectRoot status --porcelain)
if ($dirty -and -not $AllowDirty) { throw 'Commit the reviewed source first. -AllowDirty is only for local candidate verification.' }
$uvArchive = Join-Path $ToolRoot 'uv/0.11.9/uv-x86_64-pc-windows-msvc.zip'
if ((Get-FileHash -LiteralPath $uvArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'facbf9637c373761a96fa63c537d6c46581d357a65af01eacfd8c6319e6fb14e') { throw 'The pinned uv archive did not verify.' }
$basePython = Join-Path $ToolRoot 'uv-python/cpython-3.12.13-windows-x86_64-none'
if (-not (Test-Path -LiteralPath (Join-Path $basePython 'python.exe'))) { throw 'The pinned managed CPython base is required. Do not copy a virtual environment.' }
$qtCache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt')
if ($qtCache -match '^BREAKWATER_ENABLE_ASAN:BOOL=ON$') { throw 'The portable candidate must be a normal build, not ASan.' }
$qtLine = @($qtCache | Where-Object { $_ -match '^Qt6_DIR:PATH=' })
if ($qtLine.Count -ne 1) { throw 'The configured Qt SDK was not found.' }
$qtPrefix = [IO.Path]::GetFullPath((Join-Path ($qtLine[0] -replace '^Qt6_DIR:PATH=', '') '../../..'))
$candidate = Join-Path $Destination 'Breakwater'
$logs = Join-Path $Destination 'verification'
[void][IO.Directory]::CreateDirectory($candidate)
[void][IO.Directory]::CreateDirectory($logs)
$utf8 = [Text.UTF8Encoding]::new($false)
function Invoke-PackageStep([string]$Name, [string]$File, [string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new($File)
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.Environment.Clear()
    foreach ($environmentName in @('SystemRoot','WINDIR','PATH','PATHEXT','TEMP','TMP','USERPROFILE','LOCALAPPDATA',
        'INCLUDE','LIB','LIBPATH','VCToolsInstallDir','VCINSTALLDIR','WindowsSdkDir','WindowsSDKVersion',
        'WindowsSDKLibVersion','UniversalCRTSdkDir','UCRTVersion','VSINSTALLDIR','VCToolsRedistDir','VisualStudioVersion')) {
        $value = [Environment]::GetEnvironmentVariable($environmentName, 'Process')
        if ($value) { $start.Environment[$environmentName] = $value }
    }
    $start.Environment['PYTHONDONTWRITEBYTECODE'] = '1'
    $start.Environment['PYTHONNOUSERSITE'] = '1'
    $start.Environment['UV_CACHE_DIR'] = Join-Path $ToolRoot 'uv-cache'
    $start.Environment['UV_LINK_MODE'] = 'copy'
    $start.Environment['UV_NO_CONFIG'] = '1'
    $start.Environment['UV_CREDENTIALS_DIR'] = Join-Path $Destination 'installer-credentials'
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $output = $process.StandardOutput.ReadToEndAsync(); $errors = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(600000)) { $process.Kill($true); throw "Package step $Name timed out." }
    [IO.File]::WriteAllText((Join-Path $logs "$Name.log"), $output.GetAwaiter().GetResult() + $errors.GetAwaiter().GetResult(), $utf8)
    if ($process.ExitCode -ne 0) { throw "Package step $Name failed ($($process.ExitCode)). See $logs." }
}
$report = [ordered]@{ status='running'; candidate_only=$true; public_release_approved=$false; source_commit=(& git -C $projectRoot rev-parse HEAD).Trim(); dirty=$dirty; started_utc=[DateTime]::UtcNow.ToString('o'); python='3.12.13'; torch_bundled=$false; physicsnemo_bundled=$false }
try {
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
    # Reconfigure to bind the displayed revision to this source, then build.
    # Installing an arbitrary stale build directory must not masquerade as HEAD.
    Invoke-PackageStep 'configure-current-source' $cmake @('-S', $projectRoot, '-B', $BuildDirectory)
    Invoke-PackageStep 'build-current-source' $cmake @('--build', $BuildDirectory, '--config', 'RelWithDebInfo', '--parallel', '2')
    Invoke-PackageStep 'cmake-install' $cmake @('--install', $BuildDirectory, '--prefix', $candidate, '--config', 'RelWithDebInfo')
    $runtime = Join-Path $candidate 'runtime/python'
    [void][IO.Directory]::CreateDirectory($runtime)
    foreach ($item in Get-ChildItem -LiteralPath $basePython) { Copy-Item -LiteralPath $item.FullName -Destination $runtime -Recurse }
    $python = Join-Path $runtime 'python.exe'
    $installer = Join-Path $Destination 'installer'
    Expand-Archive -LiteralPath $uvArchive -DestinationPath $installer
    # Only this new, package-owned copy is modified; never the managed base.
    Invoke-PackageStep 'operations-python-lock' (Join-Path $installer 'uv.exe') @('pip','sync',(Join-Path $projectRoot 'science/requirements-worker-tests-win-py312.txt'),'--python',$python,'--break-system-packages','--require-hashes','--index-url','https://pypi.org/simple','--no-python-downloads')
    Invoke-PackageStep 'runtime-check' $python @('-B','-I','-c','import importlib.util,json,jsonschema,struct,sys; assert sys.version_info[:3]==(3,12,13) and struct.calcsize("P")==8; assert importlib.util.find_spec("torch") is None and importlib.util.find_spec("physicsnemo") is None; print(json.dumps({"python":"3.12.13","bits":64,"operations_schema_validation":True,"torch":False,"physicsnemo":False}))')
    $notices = Join-Path $candidate 'share/breakwater/licenses'
    Copy-Item -LiteralPath (Join-Path $basePython 'LICENSE.txt') -Destination (Join-Path $notices 'CPython-LICENSE.txt')
    if (Test-Path -LiteralPath (Join-Path $qtPrefix 'sbom')) { Copy-Item -LiteralPath (Join-Path $qtPrefix 'sbom') -Destination (Join-Path $notices 'Qt-SBOM') -Recurse }
    [void][IO.Directory]::CreateDirectory((Join-Path $notices 'Python-packages'))
    foreach ($dist in Get-ChildItem -LiteralPath (Join-Path $runtime 'Lib/site-packages') -Directory -Filter '*.dist-info') {
        Copy-Item -LiteralPath $dist.FullName -Destination (Join-Path $notices 'Python-packages') -Recurse -Force
    }
    foreach ($document in @('docs/PORTABLE_README.md','docs/LICENSING.md','docs/PORTFOLIO_WALKTHROUGH.md')) {
        if (-not (Test-Path -LiteralPath (Join-Path $projectRoot $document))) { throw "Required delivery documentation is missing: $document" }
        $documentName = if ($document -eq 'docs/PORTABLE_README.md') { 'README.md' } else { [IO.Path]::GetFileName($document) }
        Copy-Item -LiteralPath (Join-Path $projectRoot $document) -Destination (Join-Path $candidate $documentName)
    }
    [IO.File]::WriteAllText((Join-Path $candidate 'Start-Breakwater.cmd'), "@echo off`r`nstart `"`" `"%~dp0bin\breakwater.exe`" %*`r`n", [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $candidate 'Start-Breakwater-Science.cmd'), "@echo off`r`nif not defined BREAKWATER_PYTHON (`r`n  echo Set BREAKWATER_PYTHON to the absolute Python path from Setup-OptionalScience.ps1 first.`r`n  pause`r`n  exit /b 2`r`n)`r`nif not exist `"%BREAKWATER_PYTHON%`" (`r`n  echo The selected optional Python executable does not exist.`r`n  pause`r`n  exit /b 2`r`n)`r`nstart `"`" `"%~dp0bin\breakwater.exe`" %*`r`n", [Text.Encoding]::ASCII)
    $bundleTools = Join-Path $candidate 'tools'
    [void][IO.Directory]::CreateDirectory($bundleTools)
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'verify_portfolio_bundle.py') -Destination (Join-Path $bundleTools 'verify_bundle.py')
    [IO.File]::WriteAllText((Join-Path $candidate 'Verify-Breakwater.cmd'), "@echo off`r`n`"%~dp0runtime\python\python.exe`" -B -I `"%~dp0tools\verify_bundle.py`" `"%~dp0.`" `"%~dp0..\verification-%RANDOM%-%RANDOM%`"`r`npause`r`n", [Text.Encoding]::ASCII)
    [IO.File]::WriteAllText((Join-Path $candidate 'CANDIDATE-NOTICE.txt'), "Local verification candidate. Public binary release requires both owners' GPLv3 agreement, completed dependency notices/source delivery, and clean Windows verification. No public release has been authorized by this package helper.`r`n", $utf8)
    # A current source snapshot accompanies even a dirty local candidate. Git's
    # tracked/eligible source list excludes caches, credentials, databases and SDKs.
    $sourceRoot = Join-Path $Destination 'source'
    [void][IO.Directory]::CreateDirectory($sourceRoot)
    $sourceHashes = [ordered]@{}
    foreach ($relative in (& git -C $projectRoot ls-files --cached --others --exclude-standard)) {
        if ($relative -notmatch '^(desktop/|engine/|science/|proofs/|contracts/|scenarios/|scripts/|docs/|examples/|tests/|\.github/|[^/]+$)') { continue }
        if ($relative -match '(^|/)(\.env(?!\.example)|\.git|\.tools|out)(/|$)' -or $relative -match '\.(sqlite|db|pfx|pem|key)$') { continue }
        $source = Join-Path $projectRoot $relative
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { continue }
        $target = Join-Path $sourceRoot $relative
        [void][IO.Directory]::CreateDirectory((Split-Path -Parent $target))
        Copy-Item -LiteralPath $source -Destination $target
        $sourceHashes[$relative] = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $logs 'source-sha256.json'), ($sourceHashes | ConvertTo-Json -Depth 4), $utf8)
    Compress-Archive -LiteralPath $sourceRoot -DestinationPath (Join-Path $Destination 'Breakwater-source.zip') -CompressionLevel Optimal
    $hashes = [ordered]@{}
    foreach ($file in Get-ChildItem -LiteralPath $candidate -File -Recurse) {
        $relative = [IO.Path]::GetRelativePath($candidate, $file.FullName).Replace('\','/')
        $hashes[$relative] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $candidate 'files-sha256.json'), ($hashes | ConvertTo-Json -Depth 4), $utf8)
    $report.status='assembled'; $report.file_count=$hashes.Count; $report.source_file_count=$sourceHashes.Count
    $report.app_sha256 = $hashes['bin/breakwater.exe']
    Compress-Archive -LiteralPath $candidate -DestinationPath (Join-Path $Destination 'Breakwater-windows-x64.zip') -CompressionLevel Optimal
    $report.zip_sha256 = (Get-FileHash -LiteralPath (Join-Path $Destination 'Breakwater-windows-x64.zip') -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Output "Assembled a local candidate at $candidate. Run Verify-PortfolioBundle.ps1 before considering distribution."
} catch { $report.status='failed'; throw }
finally {
    $report.finished_utc=[DateTime]::UtcNow.ToString('o')
    [IO.File]::WriteAllText((Join-Path $logs 'package.json'), ($report | ConvertTo-Json -Depth 8), $utf8)
}
