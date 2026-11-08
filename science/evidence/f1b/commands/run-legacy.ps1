param([ValidateSet('normal','asan')][string]$Variant,[string]$Attempt)
$ErrorActionPreference='Stop'
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$env:OPENBLAS_NUM_THREADS='1';$env:OMP_NUM_THREADS='1';$env:MKL_NUM_THREADS='1';$env:PYTHONUTF8='1'
$attemptPath="out/f1b/$Attempt"
if(Test-Path -LiteralPath $attemptPath){throw 'Attempt already exists'}
New-Item -ItemType Directory -Path $attemptPath | Out-Null
$pythonExe='C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'
$build="out/f1b/$Variant";$executions=@()
foreach($study in @(@{name='refined';config='science/a9_transport_refined_config.json';expected=0},@{name='coarse';config='science/a9_transport_config.json';expected=1})){
 $started=[DateTime]::UtcNow
 $studyArgs=@('science/qualify_transport.py','--native',"$build/engine/breakwater_transport_qualification_reference.exe",'--config',$study.config,'--output',"$attemptPath/a9-$($study.name)")
 & $pythonExe -u @studyArgs *> "$attemptPath/a9-$($study.name).log"
 $code=$LASTEXITCODE
 $oldPath="C:/Projects/Breakwater/.worktrees/person-a-front-accuracy/out/f1/attempt-06-normal/a9-$($study.name)/native-reference.json"
 $newPath="$attemptPath/a9-$($study.name)/native-reference.json"
 $oldHash=(Get-FileHash -LiteralPath $oldPath -Algorithm SHA256).Hash.ToLowerInvariant()
 $newHash=(Get-FileHash -LiteralPath $newPath -Algorithm SHA256).Hash.ToLowerInvariant()
 $executions += [ordered]@{name=$study.name;executable=$pythonExe;arguments=$studyArgs;exit=$code;expected_exit=$study.expected;elapsed_s=([DateTime]::UtcNow-$started).TotalSeconds;baseline_sha256=$oldHash;new_sha256=$newHash;byte_identical=($oldHash -eq $newHash)}
 $executions | ConvertTo-Json -Depth 6 > "$attemptPath/executions.json"
 Get-Content -LiteralPath "$attemptPath/a9-$($study.name).log" -Tail 4
 if($code -ne $study.expected -or $oldHash -ne $newHash){throw 'Unexpected legacy result'}
}
