param([ValidateSet('normal','asan')][string]$Variant,[string]$Attempt)
$ErrorActionPreference='Stop'
$projectRoot=(Get-Location).Path
$attemptPath=Join-Path $projectRoot "out/f1b/$Attempt"
if(Test-Path -LiteralPath $attemptPath){throw 'Attempt already exists'}
[void][IO.Directory]::CreateDirectory($attemptPath)
$files=@('engine/experimental/front_advection.cpp','engine/experimental/front_advection.hpp','engine/examples/front_comparison_reference.cpp','engine/tests/front_experiment_tests.cpp','engine/CMakeLists.txt')
foreach($file in $files){$target=Join-Path $attemptPath "sources/$file";[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target));Copy-Item -LiteralPath $file -Destination $target}
& 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/Launch-VsDevShell.ps1' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$started=[DateTime]::UtcNow
$asan=if($Variant -eq 'asan'){'ON'}else{'OFF'}
$build="out/f1b/$Variant"
$configureArgs=@('-S','.', '-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded','-DBUILD_TESTING=ON','-DBREAKWATER_BUILD_DESKTOP=OFF',"-DBREAKWATER_ENABLE_ASAN=$asan",'-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL:NO')
cmake @configureArgs *> "$attemptPath/configure.log"
$configureExit=$LASTEXITCODE
if($configureExit -eq 0){cmake --build $build --parallel 4 *> "$attemptPath/build.log";$buildExit=$LASTEXITCODE}else{$buildExit=-1}
if($buildExit -eq 0){
 ctest --test-dir $build --show-only=json-v1 > "$attemptPath/registered-tests.json"
 ctest --test-dir $build --output-on-failure --output-junit "$attemptPath/ctest.xml" *> "$attemptPath/ctest.log"
 $testExit=$LASTEXITCODE
 foreach($exe in @('breakwater_engine_tests','breakwater_search_tests','breakwater_control_influence_tests','breakwater_transport_tests','breakwater_front_experiment_tests')){& "$build/engine/$exe.exe" *> "$attemptPath/$exe.log";if($LASTEXITCODE -ne 0){$testExit=$LASTEXITCODE}}
}else{$testExit=-1}
[ordered]@{variant=$Variant;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');configure_exit=$configureExit;build_exit=$buildExit;ctest_exit=$testExit;elapsed_s=([DateTime]::UtcNow-$started).TotalSeconds;configure_arguments=$configureArgs;cmake=(& cmake --version | Select-Object -First 1);ninja=(& ninja --version);vctools=$env:VCToolsVersion;windows_sdk=$env:WindowsSDKVersion} | ConvertTo-Json -Depth 5 > "$attemptPath/execution.json"
if(Test-Path -LiteralPath "$attemptPath/ctest.log"){Get-Content -LiteralPath "$attemptPath/ctest.log" -Tail 16}
Get-Content -LiteralPath "$attemptPath/build.log" -Tail 5
if($configureExit -ne 0 -or $buildExit -ne 0 -or $testExit -ne 0){exit 1}
