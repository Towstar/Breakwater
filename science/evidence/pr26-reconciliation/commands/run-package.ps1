$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
Set-Location -LiteralPath $projectRoot
$attempt='out/verification/reconciliation/attempt-04-package'
[void][IO.Directory]::CreateDirectory((Join-Path $projectRoot $attempt))
$export=Join-Path $projectRoot 'out/reconcile-source'
if(Test-Path -LiteralPath $export){throw 'Use a fresh disposable source export'}
git archive HEAD --format=zip -o "$attempt/source.zip"
if($LASTEXITCODE -ne 0){throw 'Source archive failed'}
Expand-Archive -LiteralPath "$attempt/source.zip" -DestinationPath $export
Get-ChildItem -LiteralPath $export -Recurse -File | ForEach-Object {
 [ordered]@{path=[IO.Path]::GetRelativePath($export,$_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}
} | ConvertTo-Json > "$attempt/export-source-hashes.json"
$locatorPath="${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$installation=& $locatorPath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
& "$installation/Common7/Tools/Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$build=Join-Path $export 'out/package'
$started=[DateTime]::UtcNow
$configureArgs=@('-S',$export,'-B',$build,'-G','Ninja','-DCMAKE_BUILD_TYPE=RelWithDebInfo',
 '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded','-DBUILD_TESTING=OFF','-DBREAKWATER_BUILD_DESKTOP=ON',
 '-DCMAKE_PREFIX_PATH=C:/Projects/Breakwater/.tools/Qt/6.12.0/msvc2022_64',
 '-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/debug /INCREMENTAL:NO')
cmake @configureArgs *> "$attempt/configure.log"
if($LASTEXITCODE -ne 0){throw 'Package configure failed'}
cmake --build $build --target breakwater --parallel 6 *> "$attempt/build.log"
if($LASTEXITCODE -ne 0){Get-Content "$attempt/build.log" -Tail 25;throw 'Package build failed'}
$installed=Join-Path $projectRoot 'out/reconcile-install-local'
cmake --install $build --prefix $installed *> "$attempt/install.log"
if($LASTEXITCODE -ne 0){Get-Content "$attempt/install.log" -Tail 25;throw 'Install failed'}
[ordered]@{started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');
 configure_arguments=$configureArgs;elapsed_s=([DateTime]::UtcNow-$started).TotalSeconds;installed=$installed;
 source_commit=(git rev-parse HEAD)} | ConvertTo-Json -Depth 5 > "$attempt/execution.json"
Get-Content "$attempt/install.log" -Tail 8
