[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('native', 'desktop', 'science', 'lean')][string]$Suite,
    [ValidateSet('normal', 'asan')][string]$Mode = 'normal',
    [switch]$SkipDependencySetup
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($Suite -in @('science', 'lean') -and $Mode -ne 'normal') { throw 'ASan mode is for compiled native/desktop checks.' }
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$artifactRoot = Join-Path $projectRoot "out/ci/$Suite-$Mode"
$buildRoot = Join-Path $projectRoot "out/ci-build/$Suite-$Mode"
$originalCiPath = $env:PATH
$utf8 = [Text.UTF8Encoding]::new($false)
[void][IO.Directory]::CreateDirectory($artifactRoot)
$script:ciSteps = [Collections.Generic.List[object]]::new()
$script:ciStep = 'initialization'
$report = [ordered]@{
    verification_version = 1; suite = $Suite; mode = $Mode; status = 'running'
    started_utc = [DateTime]::UtcNow.ToString('o')
    source_commit = (& git -C $projectRoot rev-parse HEAD).Trim()
    source_tree = (& git -C $projectRoot rev-parse 'HEAD^{tree}').Trim()
    working_tree_dirty = [bool](& git -C $projectRoot status --porcelain)
    helper_sha256 = [ordered]@{}
    runner = [ordered]@{ image = $env:ImageOS; image_version = $env:ImageVersion; os = [Environment]::OSVersion.VersionString }
    external_model_requests = 'not_run'; toolchain = [ordered]@{}; steps = $script:ciSteps
}
foreach ($helper in @('scripts/ci/Run-Checks.ps1', 'scripts/ci/Initialize-Compiler.ps1', 'scripts/ci/Write-JobEvidence.ps1', 'scripts/ci/Run-PythonTests.py', 'scripts/ci/requirements-win-py312.txt', '.github/workflows/windows-checks.yml')) {
    $report.helper_sha256[$helper] = (Get-FileHash -LiteralPath (Join-Path $projectRoot $helper) -Algorithm SHA256).Hash.ToLowerInvariant()
}
# Never enumerate process/user environments or load the developer's .env file.
Remove-Item Env:NVIDIA_API_KEY -ErrorAction SilentlyContinue
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PYTHONNOUSERSITE = '1'
$env:UV_NO_CONFIG = '1'
$env:UV_NO_PROGRESS = '1'
$env:UV_CACHE_DIR = Join-Path $projectRoot '.tools/uv-cache'

function Save-Report {
    [IO.File]::WriteAllText((Join-Path $artifactRoot 'verification.json'), ($report | ConvertTo-Json -Depth 12), $utf8)
}

function Invoke-CiStep {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Executable, [string[]]$Arguments = @())
    $script:ciStep = $Name
    $start = [DateTime]::UtcNow
    Write-Host "::group::$Name"
    $global:LASTEXITCODE = 0
    & $Executable @Arguments 2>&1 | Tee-Object -FilePath (Join-Path $artifactRoot "$Name.log")
    $exitCode = $LASTEXITCODE
    Write-Host '::endgroup::'
    $script:ciSteps.Add([ordered]@{ name = $Name; exit_code = $exitCode; elapsed_s = ([DateTime]::UtcNow - $start).TotalSeconds })
    Save-Report
    if ($exitCode -ne 0) { throw "CI step '$Name' failed with exit $exitCode; retain its log." }
}

function Enter-CiCompiler {
    $script:ciStep = 'compiler-setup'
    $report.toolchain.bootstrap_path_length_before = $env:PATH.Length
    Save-Report
    $compilerSetup = & (Join-Path $PSScriptRoot 'Initialize-Compiler.ps1')
    $report.toolchain.compiler = $compilerSetup.compiler
    $report.toolchain.visual_studio = $compilerSetup.visual_studio
    $report.toolchain.bootstrap_path_length = $compilerSetup.bootstrap_path_length
    $report.toolchain.compiler_path_length = $env:PATH.Length
    $report.toolchain.windows_sdk = $env:WindowsSDKVersion
    $report.toolchain.cmake = (& cmake --version | Select-Object -First 1)
    $report.toolchain.ninja = (& ninja --version)
    Save-Report
}

function Set-UpCiBootstrap {
    $bootstrap = Join-Path $projectRoot '.tools/ci-bootstrap'
    if (-not $SkipDependencySetup -and -not (Test-Path -LiteralPath (Join-Path $bootstrap 'Scripts/python.exe'))) {
        Invoke-CiStep 'bootstrap-venv' 'python' @('-m', 'venv', $bootstrap)
    }
    $bootstrapPython = Join-Path $bootstrap 'Scripts/python.exe'
    if (-not (Test-Path -LiteralPath $bootstrapPython)) { throw 'The existing CI bootstrap environment is unavailable.' }
    if (-not $SkipDependencySetup) {
        Invoke-CiStep 'bootstrap-lock' $bootstrapPython @('-m', 'pip', 'install', '--require-hashes', '--only-binary=:all:', '-r', (Join-Path $PSScriptRoot 'requirements-win-py312.txt'))
    }
    Invoke-CiStep 'bootstrap-versions' $bootstrapPython @('-B', '-c', 'import importlib.metadata as m,json,platform; expected={"requests":"2.34.2","py7zr":"1.1.4","PyYAML":"6.0.3"}; actual={k:m.version(k) for k in expected}; assert actual==expected, actual; print(json.dumps({"python":platform.python_version(),"packages":actual}))')
    $report.toolchain.bootstrap = Get-Content -Raw -LiteralPath (Join-Path $artifactRoot 'bootstrap-versions.log') | ConvertFrom-Json
    Invoke-CiStep 'workflow-regressions' $bootstrapPython @('-B', '-m', 'unittest', 'discover', '-s', (Join-Path $PSScriptRoot 'tests'), '-v')
    $script:ciBootstrapPython = $bootstrapPython
}

function Set-UpCiScience {
    if (-not $SkipDependencySetup) {
        Invoke-CiStep 'scientific-setup' 'pwsh' @('-NoProfile', '-File', (Join-Path $projectRoot 'scripts/Setup-ScientificWorker.ps1'))
    }
    $script:ciSciencePython = Join-Path $projectRoot '.tools/science-cpu/Scripts/python.exe'
    if (-not (Test-Path -LiteralPath $script:ciSciencePython)) { throw 'The existing CPU worker environment is unavailable.' }
    $env:BREAKWATER_PYTHON = $script:ciSciencePython
    $runtimeCheck = 'import json,platform,struct,sys,numpy,torch; assert sys.version_info[:3]==(3,12,13) and struct.calcsize("P")==8; assert torch.__version__=="2.15.0+cpu" and numpy.__version__=="2.5.4"; x=torch.tensor([2.,3.],dtype=torch.float64,device="cpu"); assert x.sum().item()==5.; print(json.dumps({"python":platform.python_version(),"torch":torch.__version__,"numpy":numpy.__version__,"device":str(x.device),"dtype":str(x.dtype)}))'
    Invoke-CiStep 'scientific-runtime' $script:ciSciencePython @('-B', '-I', '-c', $runtimeCheck)
    $report.toolchain.science = Get-Content -Raw -LiteralPath (Join-Path $artifactRoot 'scientific-runtime.log') | ConvertFrom-Json
    Save-Report
}

function Set-UpCiEogProvider {
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'science/eog_worker.py'))) { return }
    $native = Join-Path $buildRoot 'engine/breakwater_eog_response.dll'
    $contracts = Join-Path $projectRoot 'contracts/eog'
    if (-not (Test-Path -LiteralPath $native -PathType Leaf) -or
        -not (Test-Path -LiteralPath (Join-Path $contracts 'validate.py') -PathType Leaf)) {
        throw 'The real EOG worker requires the built native library and adopted local contracts.'
    }
    $env:BREAKWATER_EOG_NATIVE = $native
    $env:BREAKWATER_EOG_CONTRACTS = $contracts
    $report.toolchain.eog_provider = [ordered]@{
        native_sha256 = (Get-FileHash -LiteralPath $native -Algorithm SHA256).Hash.ToLowerInvariant()
        worker_sha256 = (Get-FileHash -LiteralPath (Join-Path $projectRoot 'science/eog_worker.py') -Algorithm SHA256).Hash.ToLowerInvariant()
        contracts = 'contracts/eog'; native_mode = $Mode
    }
    if ($Mode -eq 'asan') {
        $asanRuntime = Join-Path (Split-Path -Parent $native) 'clang_rt.asan_dynamic-x86_64.dll'
        if (-not (Test-Path -LiteralPath $asanRuntime -PathType Leaf)) { throw 'The checked EOG DLL lacks its staged matching ASan runtime.' }
        $report.toolchain.eog_provider['asan_runtime_sha256'] = (Get-FileHash -LiteralPath $asanRuntime -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    Save-Report
}

function Copy-CiBuildEvidence {
    if (-not (Test-Path -LiteralPath $buildRoot)) { return }
    foreach ($name in @('CMakeCache.txt', 'Testing/Temporary/LastTest.log', 'Testing/Temporary/LastTestsFailed.log')) {
        $source = Join-Path $buildRoot $name
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $artifactRoot ([IO.Path]::GetFileName($name))) }
    }
    $desktop = Join-Path $buildRoot 'desktop'
    if (Test-Path -LiteralPath $desktop) {
        Get-ChildItem -LiteralPath $desktop -Filter '*-results.txt' -File | Copy-Item -Destination $artifactRoot
        Get-ChildItem -LiteralPath $desktop -Filter 'science-*.json' -File | Copy-Item -Destination $artifactRoot
        $evidence = Join-Path $desktop 'test-artifacts'
        if (Test-Path -LiteralPath $evidence) { Copy-Item -LiteralPath $evidence -Destination (Join-Path $artifactRoot 'test-artifacts') -Recurse }
        $charts = Join-Path $desktop 'eog-plan-chart-artifacts'
        if (Test-Path -LiteralPath $charts) { Copy-Item -LiteralPath $charts -Destination (Join-Path $artifactRoot 'eog-plan-chart-artifacts') -Recurse }
        $details = Join-Path $desktop 'eog-plan-details-artifacts'
        if (Test-Path -LiteralPath $details) { Copy-Item -LiteralPath $details -Destination (Join-Path $artifactRoot 'eog-plan-details-artifacts') -Recurse }
        $events = Join-Path $desktop 'eog-event-details-artifacts'
        if (Test-Path -LiteralPath $events) { Copy-Item -LiteralPath $events -Destination (Join-Path $artifactRoot 'eog-event-details-artifacts') -Recurse }
    }
    foreach ($directory in @($buildRoot, $desktop)) {
        if (Test-Path -LiteralPath $directory) { Get-ChildItem -LiteralPath $directory -Filter '*.png' -File | Copy-Item -Destination $artifactRoot }
    }
}

Save-Report
Push-Location -LiteralPath $projectRoot
try {
    if ($Suite -ne 'lean') { Set-UpCiBootstrap }
    if ($Suite -in @('native', 'desktop')) {
        Enter-CiCompiler
        $configure = @('-S', $projectRoot, '-B', $buildRoot, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
            '-DBUILD_TESTING=ON', '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded',
            "-DBREAKWATER_ENABLE_ASAN=$(if ($Mode -eq 'asan') { 'ON' } else { 'OFF' })",
            "-DBREAKWATER_BUILD_DESKTOP=$(if ($Suite -eq 'desktop') { 'ON' } else { 'OFF' })")
        if ($Suite -eq 'desktop') {
            Set-UpCiScience
            if (-not $SkipDependencySetup) {
                Invoke-CiStep 'qt-sdk' $script:ciBootstrapPython @('-B', (Join-Path $projectRoot 'scripts/install-qt.py'))
            }
            $qt = Join-Path $projectRoot '.tools/Qt/6.12.0/msvc2022_64'
            $env:PATH = (Join-Path $qt 'bin') + ';' + $env:PATH
            $env:QSG_RHI_BACKEND = 'd3d11'
            $env:QSG_RHI_PREFER_SOFTWARE_RENDERER = '1'
            $report.toolchain.qt = (& (Join-Path $qt 'bin/qmake.exe') -query QT_VERSION).Trim()
            if ($report.toolchain.qt -ne '6.12.0') { throw 'The Qt SDK does not match the pinned version.' }
            $configure += @("-DCMAKE_PREFIX_PATH=$qt", "-DPython3_EXECUTABLE=$script:ciSciencePython")
        }
        Invoke-CiStep 'configure' 'cmake' $configure
        Invoke-CiStep 'build' 'cmake' @('--build', $buildRoot, '--parallel', '2')
        if ($Suite -eq 'desktop') { Set-UpCiEogProvider }
        Invoke-CiStep 'registered-tests' 'ctest' @('--test-dir', $buildRoot, '--show-only=json-v1')
        $registered = Get-Content -Raw -LiteralPath (Join-Path $artifactRoot 'registered-tests.log') | ConvertFrom-Json
        if ($registered.tests.Count -eq 0) { throw 'CTest registered no tests.' }
        if ($Suite -eq 'desktop') {
            foreach ($required in @('desktop.qml_smoke', 'desktop.storage_worker', 'desktop.storage_science', 'desktop.agent_science', 'desktop.science_evidence', 'desktop.scientific_history_normal', 'desktop.scientific_history_compact', 'desktop.pipe_evidence_smoke')) {
                if ($required -notin $registered.tests.name) { throw "Required desktop check is missing: $required" }
            }
            if (Test-Path -LiteralPath (Join-Path $projectRoot 'contracts/telemetry/validate.py')) {
                foreach ($required in @('desktop.telemetry_contracts', 'contracts.telemetry')) {
                    if ($required -notin $registered.tests.name) { throw "Required telemetry contract check is missing: $required" }
                }
            }
            if (Test-Path -LiteralPath (Join-Path $projectRoot 'desktop/telemetry/TelemetryController.hpp')) {
                foreach ($required in @('desktop.sensor_editor', 'desktop.telemetry_stream', 'desktop.telemetry_lifecycle',
                    'desktop.sensor_editor_normal_smoke', 'desktop.sensor_editor_compact_smoke',
                    'desktop.telemetry_replay_normal_smoke', 'desktop.telemetry_replay_compact_smoke')) {
                    if ($required -notin $registered.tests.name) { throw "Required G2 desktop check is missing: $required" }
                }
            }
            if (Test-Path -LiteralPath (Join-Path $projectRoot 'desktop/eog/EogDataset.hpp')) {
                foreach ($required in @('desktop.eog_dataset', 'desktop.eog_capture', 'desktop.eog_analysis_client', 'desktop.eog_history',
                    'desktop.eog_investigator', 'desktop.eog_analysis_controller', 'desktop.eog_plan_chart', 'desktop.eog_plan_details', 'desktop.eog_event_details',
                    'desktop.eog_analysis_normal', 'desktop.eog_analysis_compact',
                    'desktop.eog_cli', 'desktop.eog_overview_normal',
                    'desktop.eog_overview_compact', 'desktop.eog_facility_normal', 'desktop.eog_facility_compact')) {
                    if ($required -notin $registered.tests.name) { throw "Required EOG console check is missing: $required" }
                }
            }
            if (Test-Path -LiteralPath (Join-Path $projectRoot 'contracts/eog/validate.py')) {
                foreach ($required in @('desktop.eog_contracts', 'contracts.eog')) {
                    if ($required -notin $registered.tests.name) { throw "Required EOG contract check is missing: $required" }
                }
            }
            if (Test-Path -LiteralPath (Join-Path $projectRoot 'science/eog_worker.py')) {
                foreach ($required in @('desktop.eog_provider', 'desktop.eog_live_cli', 'desktop.eog_demo_input')) {
                    if ($required -notin $registered.tests.name) { throw "Required real EOG integration check is missing: $required" }
                }
            }
        }
        Invoke-CiStep 'ctest' 'ctest' @('--test-dir', $buildRoot, '--output-on-failure', '--no-tests=error', '--output-junit', (Join-Path $artifactRoot 'ctest-results.xml'))
        [xml]$junit = Get-Content -Raw -LiteralPath (Join-Path $artifactRoot 'ctest-results.xml')
        $report.ctest = [ordered]@{ registered = $registered.tests.Count; executed = $junit.SelectNodes('//testcase').Count; skipped = $junit.SelectNodes('//testcase/skipped').Count }
        if ($report.ctest.executed -ne $report.ctest.registered -or $report.ctest.skipped -ne 0) { throw 'CTest did not execute every registered check without skips.' }
        if ($Suite -eq 'desktop') {
            $qtCounts = @(Get-ChildItem -LiteralPath (Join-Path $buildRoot 'desktop') -Filter '*-results.txt' -File | ForEach-Object {
                $match = [regex]::Match((Get-Content -Raw -LiteralPath $_.FullName), 'Totals:\s*(\d+) passed,\s*(\d+) failed,\s*(\d+) skipped')
                if (-not $match.Success) { throw "Qt case totals missing from $($_.Name)." }
                [ordered]@{ suite = $_.BaseName; passed = [int]$match.Groups[1].Value; failed = [int]$match.Groups[2].Value; skipped = [int]$match.Groups[3].Value }
            })
            $report.qt_suites = $qtCounts
            if ($qtCounts.Count -lt 12 -or @($qtCounts | Where-Object { $_.failed -ne 0 -or $_.skipped -ne 0 }).Count -ne 0) { throw 'Required Qt suites were missing, failed or skipped.' }
        }
        $testLog = Join-Path $buildRoot 'Testing/Temporary/LastTest.log'
        if ($Mode -eq 'asan' -and (Select-String -LiteralPath $testLog -Pattern '(ERROR|SUMMARY): AddressSanitizer|AddressSanitizer:DEADLYSIGNAL' -Quiet)) { throw 'An AddressSanitizer diagnostic was retained.' }
        $report.asan_scope = 'Owned C++ targets only; prebuilt Qt/Python/Torch are outside established instrumentation coverage.'
    } elseif ($Suite -eq 'science') {
        Set-UpCiScience
        if (Test-Path -LiteralPath (Join-Path $projectRoot 'science/eog_worker.py')) {
            Enter-CiCompiler
            Invoke-CiStep 'eog-native-configure' 'cmake' @('-S', $projectRoot, '-B', $buildRoot, '-G', 'Ninja',
                '-DCMAKE_BUILD_TYPE=RelWithDebInfo', '-DBUILD_TESTING=OFF', '-DBREAKWATER_BUILD_DESKTOP=OFF',
                '-DBREAKWATER_ENABLE_ASAN=OFF', '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded')
            Invoke-CiStep 'eog-native-build' 'cmake' @('--build', $buildRoot, '--target', 'breakwater_eog_response', '--parallel', '2')
            Set-UpCiEogProvider
        }
        Invoke-CiStep 'science-tests' $script:ciSciencePython @('-B', (Join-Path $PSScriptRoot 'Run-PythonTests.py'), '--output', (Join-Path $artifactRoot 'science-results.json'))
        $report.science = Get-Content -Raw -LiteralPath (Join-Path $artifactRoot 'science-results.json') | ConvertFrom-Json
    } else {
        $script:ciStep = 'lean-setup'
        $leanRoot = Join-Path $projectRoot '.tools/lean'
        [void][IO.Directory]::CreateDirectory($leanRoot)
        $archive = Join-Path $leanRoot 'lean-4.35.0-windows.zip'
        $expectedHash = '5fc033e93e1a7bc59c5ebf311103d2c9cd18d4b3eddda4ffa3414e8adb0ef79e'
        if (-not (Test-Path -LiteralPath $archive)) {
            Invoke-WebRequest -Uri 'https://github.com/leanprover/lean4/releases/download/v4.35.0/lean-4.35.0-windows.zip' -OutFile $archive -TimeoutSec 600 -MaximumRetryCount 2 -RetryIntervalSec 3
        }
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) { throw 'The Lean archive does not match the committed official SHA256.' }
        $leanBin = Join-Path $leanRoot 'lean-4.35.0-windows/bin'
        if (-not (Test-Path -LiteralPath (Join-Path $leanBin 'lean.exe'))) { Expand-Archive -LiteralPath $archive -DestinationPath $leanRoot }
        $report.toolchain.lean_archive_sha256 = $expectedHash
        $report.toolchain.lean = (& (Join-Path $leanBin 'lean.exe') --version)
        Invoke-CiStep 'lean-audits' 'pwsh' @('-NoProfile', '-File', (Join-Path $projectRoot 'proofs/verify.ps1'), '-LeanBin', $leanBin, '-FetchCache')
    }
    if ($env:GITHUB_ACTIONS -eq 'true' -and (& git status --porcelain --untracked-files=no)) { throw 'A CI check modified tracked source; inspect the retained job logs.' }
    $report.status = 'passed'
} catch {
    $report.status = 'failed'
    $report.failed_step = $script:ciStep
    $report.failure_type = $_.Exception.GetType().FullName
    throw
} finally {
    $env:PATH = $originalCiPath
    Copy-CiBuildEvidence
    $report.finished_utc = [DateTime]::UtcNow.ToString('o')
    Save-Report
    if ($env:GITHUB_STEP_SUMMARY) {
        $summary = "### $Suite / $Mode`n`nStatus: **$($report.status)**. See verification.json, step logs and test reports in the job artifact. Hosted model requests: not run.`n"
        [IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY, $summary, $utf8)
    }
    Pop-Location
}
