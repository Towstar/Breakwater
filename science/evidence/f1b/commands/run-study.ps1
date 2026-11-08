param([ValidateSet('normal','asan')][string]$Variant,[string]$Attempt)
$ErrorActionPreference='Stop'
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$env:OPENBLAS_NUM_THREADS='1';$env:OMP_NUM_THREADS='1';$env:MKL_NUM_THREADS='1';$env:PYTHONUTF8='1'
$pythonExe='C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'
$attemptPath="out/f1b/$Attempt"
if(Test-Path -LiteralPath $attemptPath){throw 'Attempt already exists'}
New-Item -ItemType Directory -Path "$attemptPath/sources" | Out-Null
Copy-Item -LiteralPath science/compare_front_accuracy.py,science/tests/test_front_comparison.py -Destination "$attemptPath/sources"
$build="out/f1b/$Variant"
$argsForStudy=@('science/compare_front_accuracy.py','--native',"$build/engine/breakwater_front_comparison_reference.exe",'--baseline-native',"$build/engine/breakwater_transport_qualification_reference.exe",'--config','science/f1b_front_comparison_config.json','--output',"$attemptPath/study")
$started=[DateTime]::UtcNow
& $pythonExe -u @argsForStudy *> "$attemptPath/study.log"
$code=$LASTEXITCODE
[ordered]@{variant=$Variant;executable=$pythonExe;arguments=$argsForStudy;exit=$code;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');elapsed_s=([DateTime]::UtcNow-$started).TotalSeconds;source_revision=(git rev-parse HEAD)} | ConvertTo-Json -Depth 5 > "$attemptPath/execution.json"
Get-Content -LiteralPath "$attemptPath/study.log" -Tail 8
exit $code
