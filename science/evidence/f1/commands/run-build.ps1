param([ValidateSet('normal','asan')][string]$Variant)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $projectRoot
$attempt="out/f1/build-$Variant"
[void][IO.Directory]::CreateDirectory((Join-Path $projectRoot $attempt))
$locatorPath="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation=& $locatorPath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$installation/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$started=[DateTime]::UtcNow
$asan=if($Variant -eq 'asan'){'ON'}else{'OFF'}
$build="out/f1/$Variant"
$configureArgs=@('-S','.', '-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=RelWithDebInfo',
 '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded','-DBUILD_TESTING=ON','-DBREAKWATER_BUILD_DESKTOP=OFF',
 "-DBREAKWATER_ENABLE_ASAN=$asan",'-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL:NO')
cmake @configureArgs *> "$attempt/configure.log"
if($LASTEXITCODE -ne 0){throw 'Configure failed'}
cmake --build $build --parallel 4 *> "$attempt/build.log"
if($LASTEXITCODE -ne 0){Get-Content "$attempt/build.log" -Tail 30;throw 'Build failed'}
ctest --test-dir $build --show-only=json-v1 > "$attempt/registered-tests.json"
ctest --test-dir $build --output-on-failure --output-junit "$projectRoot/$attempt/ctest.xml" *> "$attempt/ctest.log"
$testExit=$LASTEXITCODE
[ordered]@{variant=$Variant;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
 ctest_exit=$testExit;elapsed_s=([DateTime]::UtcNow-$started).TotalSeconds;configure_arguments=$configureArgs;
 cmake=(& cmake --version | Select-Object -First 1);ninja=(& ninja --version);vctools=$env:VCToolsVersion;
 windows_sdk=$env:WindowsSDKVersion} | ConvertTo-Json -Depth 5 > "$attempt/execution.json"
Get-Content "$attempt/ctest.log" -Tail 6
if($testExit -ne 0){exit $testExit}
