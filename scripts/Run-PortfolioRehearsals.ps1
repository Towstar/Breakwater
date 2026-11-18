#requires -Version 7.0
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Executable,
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [string]$DatasetRoot = '',
    [string]$Objective = 'Investigate the current synthetic network, identify supported risks, and compare useful responses at low modeled cost. Select a response only with independent validation of the exact plan. Once the selected response has its own completed validation, finish with those exact returned plan and validation IDs rather than repeating identical validation calls. Choose tools from the evidence and keep advice for human review.')
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$projectRoot=Split-Path -Parent $PSScriptRoot
$Executable=(Resolve-Path -LiteralPath $Executable).Path
if (-not $DatasetRoot) { $DatasetRoot=Join-Path $projectRoot 'examples/portfolio' }
$DatasetRoot=(Resolve-Path -LiteralPath $DatasetRoot).Path
$EvidenceDirectory=[IO.Path]::GetFullPath($EvidenceDirectory)
if (Test-Path -LiteralPath $EvidenceDirectory) { throw 'Use a new evidence directory. Retain unsuccessful attempts.' }
[void][IO.Directory]::CreateDirectory($EvidenceDirectory)
$utf8=[Text.UTF8Encoding]::new($false)
$report=[ordered]@{status='running'; source_commit=(& git -C $projectRoot rev-parse HEAD).Trim(); dirty=[bool](& git -C $projectRoot status --porcelain); executable_sha256=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant(); started_utc=[DateTime]::UtcNow.ToString('o'); runs=@()}
try {
    $index=0
    foreach($demo in @('operations','operations','operations','renamed-topology')) {
        $index++; $runName="run-$index"
        $start=[Diagnostics.ProcessStartInfo]::new($Executable)
        $start.UseShellExecute=$false; $start.CreateNoWindow=$true
        $start.RedirectStandardOutput=$true; $start.RedirectStandardError=$true
        # Never print the process environment or place a credential in argv.
        if (-not $env:NVIDIA_API_KEY) {
            $replacement=[Environment]::GetEnvironmentVariable('NVIDIA_API_KEY','User')
            if ($replacement) { $start.Environment['NVIDIA_API_KEY']=$replacement }
        }
        foreach($item in @('--dataset',(Join-Path $DatasetRoot $demo),'--archive',(Join-Path $EvidenceDirectory "$runName.eog-session.json"),'--db',(Join-Path $EvidenceDirectory "$runName.sqlite"),'--live','--objective',$Objective)) { $start.ArgumentList.Add($item) }
        $timer=[Diagnostics.Stopwatch]::StartNew()
        $process=[Diagnostics.Process]::Start($start)
        $output=$process.StandardOutput.ReadToEndAsync(); $errors=$process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(450000)) { $process.Kill($true); throw "Hosted rehearsal $runName exceeded its process deadline." }
        [IO.File]::WriteAllText((Join-Path $EvidenceDirectory "$runName.log"),$output.GetAwaiter().GetResult(),$utf8)
        [IO.File]::WriteAllText((Join-Path $EvidenceDirectory "$runName-stderr.log"),$errors.GetAwaiter().GetResult(),$utf8)
        $summary=Get-Content -LiteralPath (Join-Path $EvidenceDirectory "$runName.log") -Raw | ConvertFrom-Json
        $report.runs += [ordered]@{name=$runName;dataset=$demo;exit_code=$process.ExitCode;elapsed_s=$timer.Elapsed.TotalSeconds;summary=$summary}
        [IO.File]::WriteAllText((Join-Path $EvidenceDirectory 'verification.json'),($report | ConvertTo-Json -Depth 20),$utf8)
        [pscustomobject]@{run=$runName;dataset=$demo;status=$summary.status;code=$summary.code;model_requests=$summary.model_requests;executed_tools=$summary.executed_tools} | ConvertTo-Json -Compress
    }
    $report.status=if (@($report.runs | Where-Object exit_code -ne 0).Count) {'incomplete'} else {'passed'}
} catch { $report.status='failed'; throw }
finally { $report.finished_utc=[DateTime]::UtcNow.ToString('o'); [IO.File]::WriteAllText((Join-Path $EvidenceDirectory 'verification.json'),($report | ConvertTo-Json -Depth 20),$utf8) }
if ($report.status -ne 'passed') { throw 'One or more rehearsals did not pass. Consult all retained archives and diagnostics.' }
