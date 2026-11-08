param([ValidateSet('normal','asan')][string]$Variant)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $projectRoot
$attempt=if($Variant -eq 'normal'){'out/f1/attempt-11-normal'}else{'out/f1/attempt-12-asan'}
if(Test-Path -LiteralPath $attempt){throw 'Use a fresh metric-verification attempt'}
[void][IO.Directory]::CreateDirectory((Join-Path $projectRoot $attempt))
$locatorPath="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation=& $locatorPath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$installation/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$env:OPENBLAS_NUM_THREADS='1';$env:OMP_NUM_THREADS='1';$env:MKL_NUM_THREADS='1';$env:PYTHONUTF8='1'
Remove-Item Env:NVIDIA_API_KEY -ErrorAction SilentlyContinue
$pythonExe='C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'
$start=[DateTime]::UtcNow
$operations=@(@{name='f1';arguments=@('science/qualify_front_accuracy.py','--native',"out/f1/$Variant/engine/breakwater_transport_qualification_reference.exe",'--config','science/f1_front_accuracy_config.json','--output',"$attempt/f1")})
if($Variant -eq 'normal'){$operations+=@{name='python-tests';arguments=@('-m','unittest','discover','-s','science/tests','-v')}}
$results=@()
foreach($operation in $operations){
 $began=[DateTime]::UtcNow;$operationArgs=$operation.arguments
 & $pythonExe -u @operationArgs *> "$attempt/$($operation.name).log"
 $code=$LASTEXITCODE
 $results += [ordered]@{name=$operation.name;executable=$pythonExe;arguments=$operationArgs;exit=$code;
  expected_exit=0;elapsed_s=([DateTime]::UtcNow-$began).TotalSeconds}
 $results | ConvertTo-Json -Depth 5 > "$attempt/executions.json"
 Get-Content "$attempt/$($operation.name).log" -Tail 4
 if($code -ne 0){throw "Unexpected $($operation.name) exit $code"}
}
[ordered]@{variant=$Variant;verified_revision=(git rev-parse HEAD);started_utc=$start.ToString('o');
 finished_utc=[DateTime]::UtcNow.ToString('o');elapsed_s=([DateTime]::UtcNow-$start).TotalSeconds;
 reused_native_checks=if($Variant -eq 'normal'){'attempt-06-normal'}else{'attempt-07-asan'};
 rationale='Only F1 diagnostic norm and its test changed after native/A9 checks; compiled C++ and executable bytes are unchanged.'} |
 ConvertTo-Json > "$attempt/execution.json"
