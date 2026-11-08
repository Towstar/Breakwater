[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$toolsRoot = Join-Path $projectRoot '.tools'
$uvVersion = '0.11.9'
$uvArchiveHash = 'facbf9637c373761a96fa63c537d6c46581d357a65af01eacfd8c6319e6fb14e'
$uvDirectory = Join-Path $toolsRoot "uv/$uvVersion"
$uvArchive = Join-Path $uvDirectory 'uv-x86_64-pc-windows-msvc.zip'
$uvExecutable = Join-Path $uvDirectory 'uv.exe'
$pythonInstallDirectory = Join-Path $toolsRoot 'uv-python'
$managedPython = Join-Path $pythonInstallDirectory 'cpython-3.12.13-windows-x86_64-none/python.exe'
$environmentDirectory = Join-Path $toolsRoot 'science-cpu'
$pythonExecutable = Join-Path $environmentDirectory 'Scripts/python.exe'
$logDirectory = Join-Path $toolsRoot 'science-setup'
$cacheDirectory = Join-Path $toolsRoot 'uv-cache'
$temporaryDirectory = Join-Path $toolsRoot 'tmp'
$utf8 = [Text.UTF8Encoding]::new($false)

foreach ($directory in @($uvDirectory, $pythonInstallDirectory, $logDirectory, $cacheDirectory, $temporaryDirectory)) {
    [void][IO.Directory]::CreateDirectory($directory)
}

# Process-local settings keep installers, managed Python, caches and temporary files
# inside the ignored checkout. This script never edits PATH, profiles or the registry.
$childEnvironment = @{
    UV_PYTHON_INSTALL_DIR = $pythonInstallDirectory
    UV_PYTHON_CACHE_DIR = (Join-Path $toolsRoot 'uv-python-cache')
    UV_PYTHON_BIN_DIR = (Join-Path $toolsRoot 'uv-python-bin')
    UV_PYTHON_INSTALL_BIN = '0'
    UV_PYTHON_INSTALL_REGISTRY = '0'
    UV_PYTHON_NO_REGISTRY = '1'
    UV_CACHE_DIR = $cacheDirectory
    UV_LINK_MODE = 'copy'
    UV_CREDENTIALS_DIR = (Join-Path $toolsRoot 'uv-credentials')
    UV_NO_CONFIG = '1'
    UV_NO_PROGRESS = '1'
    PIP_CACHE_DIR = (Join-Path $toolsRoot 'pip-cache')
    PYTHONDONTWRITEBYTECODE = '1'
    PYTHONNOUSERSITE = '1'
    TEMP = $temporaryDirectory
    TMP = $temporaryDirectory
}

function Invoke-SetupProcess {
    param([string]$Executable, [string[]]$Arguments, [string]$LogName)
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $Executable
    $start.WorkingDirectory = $projectRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    if ($start.PSObject.Properties['ArgumentList']) {
        foreach ($argument in $Arguments) { [void]$start.ArgumentList.Add($argument) }
    } else {
        # Windows PowerShell 5.1 has no ProcessStartInfo.ArgumentList. Preserve
        # embedded quotes and trailing backslashes using Windows argv quoting.
        $start.Arguments = (@($Arguments | ForEach-Object {
            $escaped = [regex]::Replace($_, '(\\*)"', '$1$1\"')
            '"' + [regex]::Replace($escaped, '(\\+)$', '$1$1') + '"'
        }) -join ' ')
    }
    foreach ($name in @('NVIDIA_API_KEY', 'PYTHONHOME', 'PYTHONPATH', 'VIRTUAL_ENV', 'CONDA_PREFIX', 'UV_PYTHON')) {
        $start.EnvironmentVariables.Remove($name)
    }
    foreach ($entry in $childEnvironment.GetEnumerator()) { $start.EnvironmentVariables[$entry.Key] = $entry.Value }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    try {
        if (-not $process.Start()) { throw "Could not start the local setup step: $LogName" }
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $output = $stdout.GetAwaiter().GetResult()
        $diagnostics = $stderr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $logDirectory $LogName), $output + $diagnostics, $utf8)
        if ($process.ExitCode -ne 0) {
            throw "Scientific setup step failed (exit $($process.ExitCode)). See $(Join-Path $logDirectory $LogName)."
        }
        return $output.Trim()
    } finally { $process.Dispose() }
}

if (-not (Test-Path -LiteralPath $uvArchive -PathType Leaf)) {
    Write-Host "Downloading project-local uv $uvVersion..."
    $download = "$uvArchive.download"
    Invoke-WebRequest -Uri "https://releases.astral.sh/github/uv/releases/download/$uvVersion/uv-x86_64-pc-windows-msvc.zip" -OutFile $download -UseBasicParsing -TimeoutSec 180
    if ((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash.ToLowerInvariant() -ne $uvArchiveHash) {
        throw 'The downloaded uv archive did not match its pinned official SHA256; it was not executed.'
    }
    Move-Item -LiteralPath $download -Destination $uvArchive
}
if ((Get-FileHash -LiteralPath $uvArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $uvArchiveHash) {
    throw 'The cached uv archive does not match its pinned official SHA256; it was not executed.'
}
Expand-Archive -LiteralPath $uvArchive -DestinationPath $uvDirectory -Force
if (-not (Test-Path -LiteralPath $uvExecutable -PathType Leaf)) { throw 'The verified archive did not contain uv.exe.' }
$actualUvVersion = Invoke-SetupProcess $uvExecutable @('--version') 'uv-version.log'
if ($actualUvVersion -notmatch '^uv 0\.11\.9(?:\s|$)') { throw 'The extracted uv executable has an unexpected version.' }

Write-Host 'Installing/checking isolated CPython 3.12.13 AMD64...'
if (-not (Test-Path -LiteralPath $managedPython -PathType Leaf)) {
    try {
        [void](Invoke-SetupProcess $uvExecutable @('python', 'install', 'cpython-3.12.13-windows-x86_64-none', '--no-bin', '--no-registry') 'python-install.log')
    } catch {
        # Restricted Windows hosts can forbid uv's optional minor-version junction
        # after the pinned interpreter was downloaded and installed successfully.
        # Use its concrete versioned path; never fall back to a different Python.
        $installLog = Get-Content -Raw -LiteralPath (Join-Path $logDirectory 'python-install.log')
        if (-not (Test-Path -LiteralPath $managedPython -PathType Leaf) -or
            -not $installLog.Contains('Failed to create Python minor version link directory')) { throw }
        Write-Host 'Python minor-version shortcut is unavailable; using the exact versioned interpreter path.'
    }
}
if (-not (Test-Path -LiteralPath $pythonExecutable -PathType Leaf)) {
    if (Test-Path -LiteralPath $environmentDirectory) {
        throw "An incomplete scientific environment exists at $environmentDirectory; preserve or repair it before retrying."
    }
    [void](Invoke-SetupProcess $uvExecutable @('venv', $environmentDirectory, '--python', $managedPython, '--no-python-downloads') 'venv.log')
}
$interpreterCheck = 'import pathlib,platform,struct,sys; assert platform.python_implementation()=="CPython" and sys.version_info[:3]==(3,12,13) and struct.calcsize("P")==8 and platform.machine().lower() in ("amd64","x86_64"), "Expected CPython 3.12.13 AMD64"; assert pathlib.Path(sys.base_prefix).resolve()==pathlib.Path(sys.argv[1]).parent.resolve() and pathlib.Path(sys.prefix).resolve()==pathlib.Path(sys.argv[2]).resolve(), "Environment must use the project-local managed interpreter"; print(sys.executable)'
[void](Invoke-SetupProcess $pythonExecutable @('-B', '-I', '-c', $interpreterCheck, $managedPython, $environmentDirectory) 'python-version.log')

Write-Host 'Synchronizing the committed CPU hash lock...'
$cpuLock = Join-Path $projectRoot 'science/requirements-win-cpu-py312.txt'
$testLock = Join-Path $projectRoot 'science/requirements-worker-tests-win-py312.txt'
[void](Invoke-SetupProcess $uvExecutable @('pip', 'sync', $cpuLock, '--python', $pythonExecutable, '--require-hashes', '--only-binary', ':all:', '--index-url', 'https://download.pytorch.org/whl/cpu', '--no-python-downloads') 'cpu-lock.log')
# Add schema test dependencies without syncing away the already pinned Torch runtime.
[void](Invoke-SetupProcess $uvExecutable @('pip', 'install', '-r', $testLock, '--python', $pythonExecutable, '--require-hashes', '--only-binary', ':all:', '--index-url', 'https://pypi.org/simple', '--no-python-downloads') 'worker-tests-lock.log')

$runtimeCheck = @'
import importlib.metadata,json,platform,struct,sys
import numpy,torch
assert sys.version_info[:3] == (3,12,13) and struct.calcsize("P") == 8
assert torch.__version__ == "2.15.0+cpu", "Expected the committed CPU Torch version"
assert numpy.__version__ == "2.5.4", "Expected the committed NumPy version"
value = torch.tensor([[1.0,2.0]], dtype=torch.float64, device="cpu") @ torch.tensor([[3.0],[4.0]], dtype=torch.float64, device="cpu")
assert value.dtype == torch.float64 and value.device.type == "cpu" and value.item() == 11.0
print(json.dumps({"python_executable":sys.executable,"python_version":platform.python_version(),"architecture":platform.machine(),"torch_version":torch.__version__,"numpy_version":numpy.__version__,"jsonschema_version":importlib.metadata.version("jsonschema"),"dtype":str(value.dtype),"device":str(value.device),"cpu_matrix_result":value.item()}))
'@
$runtime = Invoke-SetupProcess $pythonExecutable @('-B', '-I', '-c', $runtimeCheck) 'runtime-check.log' | ConvertFrom-Json
$runtime | Add-Member -NotePropertyName uv_version -NotePropertyValue $uvVersion
$runtime | Add-Member -NotePropertyName uv_archive_sha256 -NotePropertyValue $uvArchiveHash
$runtime | Add-Member -NotePropertyName cpu_lock_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $cpuLock -Algorithm SHA256).Hash.ToLowerInvariant()
$runtime | Add-Member -NotePropertyName worker_test_lock_sha256 -NotePropertyValue (Get-FileHash -LiteralPath $testLock -Algorithm SHA256).Hash.ToLowerInvariant()
$runtime | Add-Member -NotePropertyName checked_utc -NotePropertyValue ([DateTime]::UtcNow.ToString('o'))
[IO.File]::WriteAllText((Join-Path $logDirectory 'runtime.json'), ($runtime | ConvertTo-Json -Depth 4), $utf8)
Write-Host "Scientific Python ready: $pythonExecutable"
$runtime
