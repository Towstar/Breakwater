# Retain runner/setup context even if bootstrap installation fails before tests.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('native', 'desktop', 'science', 'lean')][string]$Suite,
    [ValidateSet('normal', 'asan')][string]$Mode = 'normal',
    [ValidateSet('start', 'finish')][string]$Phase = 'start',
    [ValidateSet('running', 'success', 'failure', 'cancelled')][string]$Outcome = 'running'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$artifactRoot = Join-Path $projectRoot "out/ci/$Suite-$Mode"
[void][IO.Directory]::CreateDirectory($artifactRoot)
$report = [ordered]@{
    evidence_version = 1; phase = $Phase; suite = $Suite; mode = $Mode
    recorded_utc = [DateTime]::UtcNow.ToString('o'); job_outcome = $Outcome
    source_commit = (& git -C $projectRoot rev-parse HEAD).Trim()
    bootstrap_python_requested = '3.12.10'
    scientific_python = '3.12.13 (separately managed by Setup-ScientificWorker.ps1)'
    runner = [ordered]@{ image = $env:ImageOS; image_version = $env:ImageVersion; os = $env:RUNNER_OS }
    github = [ordered]@{
        repository = $env:GITHUB_REPOSITORY; event = $env:GITHUB_EVENT_NAME
        run_id = $env:GITHUB_RUN_ID; run_attempt = $env:GITHUB_RUN_ATTEMPT
        job = $env:GITHUB_JOB; workflow = $env:GITHUB_WORKFLOW
    }
    verification_available = Test-Path -LiteralPath (Join-Path $artifactRoot 'verification.json')
    ctest_results_available = Test-Path -LiteralPath (Join-Path $artifactRoot 'ctest-results.xml')
    science_results_available = Test-Path -LiteralPath (Join-Path $artifactRoot 'science-results.json')
    test_execution_status = 'not_established'
}
if (-not $report.verification_available) { $report.test_execution_status = 'not_started' }
elseif ($report.ctest_results_available -or $report.science_results_available) { $report.test_execution_status = 'consult_test_reports' }
$file = Join-Path $artifactRoot "job-$Phase.json"
[IO.File]::WriteAllText($file, ($report | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Output "Job $Phase evidence retained for $Suite/$Mode ($Outcome)."
