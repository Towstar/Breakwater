param([ValidateSet('normal','asan')][string]$Variant)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $projectRoot
$attempt=if($Variant -eq 'normal'){'out/f1/attempt-06-normal'}else{'out/f1/attempt-07-asan'}
if(Test-Path -LiteralPath $attempt){throw 'Use a fresh verification attempt'}
[void][IO.Directory]::CreateDirectory((Join-Path $projectRoot $attempt))
$locatorPath="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation=& $locatorPath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$installation/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$env:OPENBLAS_NUM_THREADS='1';$env:OMP_NUM_THREADS='1';$env:MKL_NUM_THREADS='1';$env:PYTHONUTF8='1'
Remove-Item Env:NVIDIA_API_KEY -ErrorAction SilentlyContinue
$pythonExe='C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'
$build="out/f1/$Variant"
$start=[DateTime]::UtcNow
cmake --build $build --parallel 4 *> "$attempt/build.log"
if($LASTEXITCODE -ne 0){throw 'Final native build failed'}
ctest --test-dir $build --show-only=json-v1 > "$attempt/registered-tests.json"
$registration=Get-Content "$attempt/registered-tests.json" -Raw | ConvertFrom-Json
if($registration.tests.Count -ne 8){throw 'Expected eight headless suites'}
ctest --test-dir $build --output-on-failure --output-junit "$projectRoot/$attempt/ctest.xml" *> "$attempt/ctest.log"
if($LASTEXITCODE -ne 0){Get-Content "$attempt/ctest.log" -Tail 25;throw 'Native checks failed'}
Copy-Item -LiteralPath "$build/Testing/Temporary/LastTest.log" -Destination "$attempt/native-individual.log"
$operations=@(
 @{name='f1';arguments=@('science/qualify_front_accuracy.py','--native',"$build/engine/breakwater_transport_qualification_reference.exe",'--config','science/f1_front_accuracy_config.json','--output',"$attempt/f1");expected=0},
 @{name='a9-refined';arguments=@('science/qualify_transport.py','--native',"$build/engine/breakwater_transport_qualification_reference.exe",'--config','science/a9_transport_refined_config.json','--output',"$attempt/a9-refined");expected=0},
 @{name='a9-coarse';arguments=@('science/qualify_transport.py','--native',"$build/engine/breakwater_transport_qualification_reference.exe",'--config','science/a9_transport_config.json','--output',"$attempt/a9-coarse");expected=1}
)
if($Variant -eq 'normal'){
 $operations+=@{name='python-tests';arguments=@('-m','unittest','discover','-s','science/tests','-v');expected=0}
}
$results=@()
foreach($operation in $operations){
 $began=[DateTime]::UtcNow;$operationArgs=$operation.arguments
 & $pythonExe -u @operationArgs *> "$attempt/$($operation.name).log"
 $code=$LASTEXITCODE
 $results += [ordered]@{name=$operation.name;executable=$pythonExe;arguments=$operationArgs;exit=$code;
  expected_exit=$operation.expected;elapsed_s=([DateTime]::UtcNow-$began).TotalSeconds}
 $results | ConvertTo-Json -Depth 5 > "$attempt/executions.json"
 Get-Content "$attempt/$($operation.name).log" -Tail 4
 if($code -ne $operation.expected){throw "Unexpected $($operation.name) exit $code"}
}
[ordered]@{variant=$Variant;verified_revision=(git rev-parse HEAD);started_utc=$start.ToString('o');
 finished_utc=[DateTime]::UtcNow.ToString('o');elapsed_s=([DateTime]::UtcNow-$start).TotalSeconds;
 registered_tests=$registration.tests.Count;ctest_exit=0} | ConvertTo-Json > "$attempt/execution.json"
Get-Content "$attempt/ctest.log" -Tail 5
