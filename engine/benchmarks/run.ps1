param(
    [switch]$SkipBuild,
    [switch]$Influence,
    [ValidateSet(0, 100, 1000, 2048, 10000)][int]$Count = 0,
    [ValidateRange(3, 31)][int]$Repeats = 7,
    [ValidateRange(1, 10)][int]$Warmups = 2
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$increment = if ($Influence) { 'a7' } else { 'a5' }
$incrementLabel = $increment.ToUpperInvariant()
$buildDirectory = Join-Path $projectRoot "out/$increment-release"
$outputDirectory = Join-Path $projectRoot "out/$increment-benchmarks"
$resultSuffix = if ($Count -gt 0) { "-$Count" } else { '' }
$resultsPath = Join-Path $outputDirectory "results$resultSuffix.json"
$environmentPath = Join-Path $outputDirectory "environment$resultSuffix.json"
$executablePath = Join-Path $buildDirectory 'engine/breakwater_benchmark.exe'
$locatorPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

# Match the repository's supported x64 MSVC initialization without editing root scripts.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    if (-not (Test-Path -LiteralPath $locatorPath)) {
        throw 'Install Visual Studio C++ tools, or run in its x64 developer shell.'
    }
    $installation = & $locatorPath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installation) {
        throw 'Visual Studio x64 C++ tools were not found.'
    }
    & "$installation\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
}

Push-Location -LiteralPath $projectRoot
try {
    if (-not $SkipBuild) {
        & cmake -S $projectRoot -B $buildDirectory -G Ninja `
            -DCMAKE_BUILD_TYPE=Release -DBREAKWATER_BUILD_DESKTOP=OFF `
            -DBUILD_TESTING=ON -DBREAKWATER_ENABLE_ASAN=OFF `
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        if ($LASTEXITCODE -ne 0) { throw "$incrementLabel Release configuration failed." }
        & cmake --build $buildDirectory
        if ($LASTEXITCODE -ne 0) { throw "$incrementLabel Release build failed." }
    }

    $cachePath = Join-Path $buildDirectory 'CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $executablePath) -or -not (Test-Path -LiteralPath $cachePath)) {
        throw "The $incrementLabel Release build is missing. Run again without -SkipBuild."
    }
    $cache = Get-Content -LiteralPath $cachePath
    if (-not ($cache -match '^CMAKE_BUILD_TYPE:STRING=Release$') -or
        -not ($cache -match '^BREAKWATER_ENABLE_ASAN:BOOL=OFF$')) {
        throw 'Performance measurements require the separate Release build with ASan OFF.'
    }

    # Even -SkipBuild verifies the reused build before collecting measurements.
    & ctest --test-dir $buildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "$incrementLabel Release correctness checks failed." }
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

    $sourceCommit = & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record the benchmark source commit.' }
    $sourceStatus = @(& git status --porcelain)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record benchmark source status.' }
    $cmakeVersion = @(& cmake --version)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot record the CMake version.' }

    $cpuBrand = $null
    $cpuBrandError = $null
    try {
        $cpuBrand = (Get-ItemProperty -LiteralPath 'HKLM:\HARDWARE\DESCRIPTION\System\CentralProcessor\0' -Name ProcessorNameString).ProcessorNameString.Trim()
    } catch {
        $cpuBrandError = $_.Exception.Message
    }
    $logicalProcessors = $null
    if ($env:NUMBER_OF_PROCESSORS) {
        $parsedProcessors = 0
        if ([int]::TryParse($env:NUMBER_OF_PROCESSORS, [ref]$parsedProcessors)) {
            $logicalProcessors = $parsedProcessors
        }
    }

    $sourcePaths = @(
        'engine/benchmarks/benchmark.cpp',
        'engine/benchmarks/timing.hpp',
        'engine/benchmarks/workloads.hpp',
        'engine/benchmarks/run.ps1',
        'engine/src/forecast_kernels.hpp',
        'engine/src/search_kernel.hpp',
        'engine/src/control_influence.hpp',
        'engine/src/search_evaluation.hpp',
        'engine/src/engine.cpp',
        'engine/src/search.cpp',
        'engine/src/scenario.cpp',
        'engine/include/breakwater/engine.hpp',
        'engine/include/breakwater/search.hpp',
        'engine/CMakeLists.txt',
        'contracts/include/breakwater/types.hpp',
        'CMakeLists.txt'
    )
    $sourceHashes = @(
        foreach ($relativePath in $sourcePaths) {
            $fullPath = Join-Path $projectRoot $relativePath
            if (-not (Test-Path -LiteralPath $fullPath)) {
                throw "Cannot hash required benchmark source: $relativePath"
            }
            [ordered]@{
                path = $relativePath
                sha256 = (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
    )
    $compileCommandsPath = Join-Path $buildDirectory 'compile_commands.json'
    if (-not (Test-Path -LiteralPath $compileCommandsPath)) {
        throw 'Compilation command evidence is missing. Reconfigure without -SkipBuild.'
    }
    $environmentRecord = [ordered]@{
        schema_version = 1
        status = 'running'
        started_utc = [DateTime]::UtcNow.ToString('o')
        source_commit = "$sourceCommit".Trim()
        source_dirty = ($sourceStatus.Count -gt 0)
        os = [Environment]::OSVersion.VersionString
        os_architecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
        process_architecture = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString()
        logical_processors_environment = $logicalProcessors
        logical_processors_dotnet = [Environment]::ProcessorCount
        cpu_brand = $cpuBrand
        cpu_brand_read_error = $cpuBrandError
        powershell_version = $PSVersionTable.PSVersion.ToString()
        cmake_version = $cmakeVersion[0]
        executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash.ToLowerInvariant()
        compile_commands_sha256 = (Get-FileHash -LiteralPath $compileCommandsPath -Algorithm SHA256).Hash.ToLowerInvariant()
        source_hashes = $sourceHashes
        configuration = [ordered]@{
            build_type = 'Release'
            generator = 'Ninja'
            asan = $false
            desktop = $false
            repeats = $Repeats
            warmups = $Warmups
            candidates = @(if ($Count -gt 0) { $Count } else { 100, 1000, 10000 })
            workloads = @('canonical_incident', 'custom_24_tanks')
            horizon_h = 3.0
            sample_step_h = 0.025
            skip_build = [bool]$SkipBuild
            influence_comparison = [bool]$Influence
        }
        cmake_cache_evidence = @($cache | Where-Object {
            $_ -match '^(CMAKE_BUILD_TYPE|CMAKE_CXX_COMPILER|CMAKE_CXX_FLAGS|CMAKE_CXX_FLAGS_RELEASE|BREAKWATER_ENABLE_ASAN|BREAKWATER_BUILD_DESKTOP|BUILD_TESTING):'
        })
    }
    $utf8 = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllText($environmentPath, ($environmentRecord | ConvertTo-Json -Depth 8) + [Environment]::NewLine, $utf8)

    try {
        $benchmarkArguments = @('--repeats', $Repeats, '--warmups', $Warmups, '--output', $resultsPath)
        if ($Count -gt 0) { $benchmarkArguments += @('--count', $Count) }
        & $executablePath @benchmarkArguments
        if ($LASTEXITCODE -ne 0) { throw "$incrementLabel benchmark failed with exit code $LASTEXITCODE." }
        if (-not (Test-Path -LiteralPath $resultsPath)) { throw 'Benchmark did not write its result artifact.' }
        $environmentRecord.status = 'completed'
    } catch {
        $environmentRecord.status = 'failed'
        $environmentRecord['error'] = $_.Exception.Message
        throw
    } finally {
        $environmentRecord['finished_utc'] = [DateTime]::UtcNow.ToString('o')
        [System.IO.File]::WriteAllText($environmentPath, ($environmentRecord | ConvertTo-Json -Depth 8) + [Environment]::NewLine, $utf8)
    }
    Write-Output "Benchmark results: $resultsPath"
    Write-Output "Environment evidence: $environmentPath"
} finally {
    Pop-Location
}
