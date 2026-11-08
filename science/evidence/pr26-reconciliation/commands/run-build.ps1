param([ValidateSet('normal','asan')][string]$Variant)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location -LiteralPath $projectRoot
$attempt="out/verification/reconciliation/attempt-02-$Variant"
[void][IO.Directory]::CreateDirectory((Join-Path $projectRoot $attempt))
$locatorPath="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation=& $locatorPath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$installation/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$env:PATH='C:/Projects/Breakwater/.tools/Qt/6.12.0/msvc2022_64/bin;'+$env:PATH
$env:BREAKWATER_PYTHON='C:/Projects/Breakwater/.worktrees/person-a-engine/science/.venv/Scripts/python.exe'
$build="out/reconcile-$Variant"
$asan=if($Variant -eq 'asan'){'ON'}else{'OFF'}
$started=[DateTime]::UtcNow
$configureArgs=@('-S','.', '-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=RelWithDebInfo',
 '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded','-DBUILD_TESTING=ON','-DBREAKWATER_BUILD_DESKTOP=ON',
 '-DCMAKE_PREFIX_PATH=C:/Projects/Breakwater/.tools/Qt/6.12.0/msvc2022_64',
 "-DPython3_EXECUTABLE=$env:BREAKWATER_PYTHON","-DBREAKWATER_ENABLE_ASAN=$asan",
 '-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL:NO')
cmake @configureArgs *> "$attempt/configure.log"
if($LASTEXITCODE -ne 0){Get-Content "$attempt/configure.log" -Tail 25;throw 'Configure failed'}
cmake --build $build --parallel 6 *> "$attempt/build.log"
if($LASTEXITCODE -ne 0){Get-Content "$attempt/build.log" -Tail 35;throw 'Build failed'}
ctest --test-dir $build --show-only=json-v1 > "$attempt/registered-tests.json"
$registration=Get-Content "$attempt/registered-tests.json" -Raw | ConvertFrom-Json
if($registration.tests.Count -ne 28){throw "Expected 28 tests, got $($registration.tests.Count)"}
cmake --build $build --target breakwater_qmllint *> "$attempt/qmllint.log"
$lintExit=$LASTEXITCODE
ctest --test-dir $build --output-on-failure --output-junit "$projectRoot/$attempt/ctest.xml" *> "$attempt/ctest.log"
$testExit=$LASTEXITCODE
Get-ChildItem -LiteralPath "$build/desktop" -Filter '*-results.txt' | Copy-Item -Destination $attempt
Get-ChildItem -LiteralPath "$build/desktop" -Filter 'science-*.json' | Copy-Item -Destination $attempt
[ordered]@{started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');variant=$Variant;
 registered_tests=$registration.tests.Count;qmllint_exit=$lintExit;ctest_exit=$testExit;
 configure_arguments=$configureArgs;elapsed_s=([DateTime]::UtcNow-$started).TotalSeconds} |
 ConvertTo-Json -Depth 6 > "$attempt/execution.json"
Get-Content "$attempt/ctest.log" -Tail 36
if($lintExit -ne 0 -or $testExit -ne 0){exit 1}
