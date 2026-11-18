#requires -Version 7.0
[CmdletBinding()]
param([string]$ToolRoot = '', [switch]$InstallOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $ToolRoot) { $ToolRoot = Join-Path $projectRoot '.tools' }
$ToolRoot = [IO.Path]::GetFullPath($ToolRoot)
$uv = Join-Path $ToolRoot 'uv/0.11.9/uv.exe'
$archive = Join-Path $ToolRoot 'uv/0.11.9/uv-x86_64-pc-windows-msvc.zip'
$managedPython = Join-Path $ToolRoot 'uv-python/cpython-3.12.13-windows-x86_64-none/python.exe'
if (-not (Test-Path -LiteralPath $uv) -or -not (Test-Path -LiteralPath $managedPython)) {
    if ($PSBoundParameters.ContainsKey('ToolRoot')) { throw 'Run Setup-ScientificWorker.ps1 in the supplied tool root first.' }
    & (Join-Path $PSScriptRoot 'Setup-ScientificWorker.ps1')
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'facbf9637c373761a96fa63c537d6c46581d357a65af01eacfd8c6319e6fb14e') {
    throw 'The optional installer requires the existing verified uv pin.'
}
$verifiedTools = Join-Path $projectRoot '.tools/optional-installer'
Expand-Archive -LiteralPath $archive -DestinationPath $verifiedTools -Force
$uv = Join-Path $verifiedTools 'uv.exe'
$environment = Join-Path $projectRoot '.tools/science-physicsnemo'
$python = Join-Path $environment 'Scripts/python.exe'
$logs = Join-Path $projectRoot 'out/optional-science'
[void][IO.Directory]::CreateDirectory($logs)
$report = [ordered]@{ status = 'running'; qualification = 'not_run'; started_utc = [DateTime]::UtcNow.ToString('o'); device = 'cpu' }
$utf8 = [Text.UTF8Encoding]::new($false)
function Invoke-OptionalStep([string]$Name, [string]$File, [string[]]$Arguments) {
    $start = [Diagnostics.ProcessStartInfo]::new($File)
    $start.UseShellExecute = $false; $start.CreateNoWindow = $true
    $start.WorkingDirectory = $projectRoot
    $start.RedirectStandardOutput = $true; $start.RedirectStandardError = $true
    $start.Environment.Clear()
    foreach ($environmentName in @('SystemRoot','WINDIR','PATH','PATHEXT','TEMP','TMP','USERPROFILE','LOCALAPPDATA')) {
        $value = [Environment]::GetEnvironmentVariable($environmentName, 'Process')
        if ($value) { $start.Environment[$environmentName] = $value }
    }
    $start.Environment['UV_CACHE_DIR'] = Join-Path $ToolRoot 'uv-cache'
    $start.Environment['UV_LINK_MODE'] = 'copy'
    $start.Environment['UV_NO_PROGRESS'] = '1'
    $start.Environment['UV_NO_CONFIG'] = '1'
    $start.Environment['UV_CREDENTIALS_DIR'] = Join-Path $projectRoot '.tools/optional-installer-credentials'
    $start.Environment['PYTHONDONTWRITEBYTECODE'] = '1'
    $start.Environment['PYTHONNOUSERSITE'] = '1'
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::Start($start)
    $output = $process.StandardOutput.ReadToEndAsync(); $errors = $process.StandardError.ReadToEndAsync()
    $deadline = [Diagnostics.Stopwatch]::StartNew()
    while (-not $process.WaitForExit(1000)) {
        if ($deadline.Elapsed.TotalMinutes -gt 20) { $process.Kill($true); throw "Optional setup step $Name timed out." }
    }
    [IO.File]::WriteAllText((Join-Path $logs "$Name.log"), $output.GetAwaiter().GetResult() + $errors.GetAwaiter().GetResult(), $utf8)
    if ($process.ExitCode -ne 0) { throw "Optional setup step $Name failed with exit $($process.ExitCode). See $logs." }
}
try {
    if (-not (Test-Path -LiteralPath $python)) { Invoke-OptionalStep 'venv' $uv @('venv', $environment, '--python', $managedPython, '--no-python-downloads') }
    $lock = Join-Path $projectRoot 'science/requirements-physicsnemo-win-cpu-py312.txt'
    Invoke-OptionalStep 'physicsnemo-lock' $uv @('pip', 'sync', $lock, '--python', $python, '--require-hashes', '--torch-backend', 'cpu', '--no-python-downloads')
    Invoke-OptionalStep 'worker-lock' $uv @('pip', 'install', '-r', (Join-Path $projectRoot 'science/requirements-worker-tests-win-py312.txt'), '--python', $python, '--require-hashes', '--index-url', 'https://pypi.org/simple', '--no-python-downloads')
    Invoke-OptionalStep 'versions' $python @('-B', '-I', '-c', 'import importlib.metadata as m,json,struct,sys,torch; assert sys.version_info[:3]==(3,12,13) and struct.calcsize("P")==8; assert torch.__version__=="2.15.0+cpu" and m.version("nvidia-physicsnemo")=="2.2.2"; import jsonschema; print(json.dumps({"python":"3.12.13","torch":torch.__version__,"physicsnemo":m.version("nvidia-physicsnemo"),"device":"cpu"}))')
    $report.lock_sha256 = (Get-FileHash -LiteralPath $lock -Algorithm SHA256).Hash.ToLowerInvariant()
    if (-not $InstallOnly) {
        Invoke-OptionalStep 'calibration-qualification' $python @('-B', (Join-Path $projectRoot 'science/qualify_transport_calibration.py'), '-v')
        $report.qualification = 'passed'
    }
    $report.status = 'passed'
    Write-Output "Optional CPU Science is ready at $python. Use Start-Portfolio.ps1 -Science for the source build."
} catch {
    $report.status = 'failed'; throw
} finally {
    $report.finished_utc = [DateTime]::UtcNow.ToString('o')
    [IO.File]::WriteAllText((Join-Path $logs 'verification.json'), ($report | ConvertTo-Json -Depth 5) + [Environment]::NewLine, $utf8)
}
