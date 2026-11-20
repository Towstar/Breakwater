param([ValidateSet('normal','asan')][string]$Mode,[switch]$Incremental)
$ErrorActionPreference='Stop'
$build = if ($Mode -eq 'asan') {'out/portfolio-desktop-asan'} else {'out/portfolio-desktop'}
# A cold/full build retains the accepted 8 GiB gate. The final shortcut-only
# increment reuses existing dependencies and compiler outputs; bound it to the
# three desktop targets and retain 4 GiB free (over twice the measured complete
# object/output footprint) rather than treating it as a new release build.
if ($Incremental -and (!(Test-Path -LiteralPath "$build/build.ninja") -or !(Test-Path -LiteralPath "$build/CMakeCache.txt"))) { throw 'Incremental mode requires the existing configured build.' }
$minimum = if ($Incremental) {4GB} else {8GB}
if ((Get-PSDrive C).Free -lt $minimum) { throw 'Below the starting resource reserve for this build scope.' }
$source = (git rev-parse --short HEAD).Trim()
$log = "out/workspace-framing/build-$Mode-$source.log"
if (Test-Path -LiteralPath $log) { throw 'Preserve the existing build log; choose a new attempt name.' }
try {
    & {
        . ./scripts/ci/Initialize-Compiler.ps1
        $env:NVIDIA_API_KEY=''
        $env:PYTHONDONTWRITEBYTECODE='1'
        $env:BREAKWATER_PYTHON='C:\Users\lucas\Documents\Codex\2026-11-07\for-x20\work\Breakwater\.tools\science-cpu\Scripts\python.exe'
        Write-Output ([pscustomobject]@{scope=if($Incremental){'Existing desktop targets only; not a cold/full/release build'}else{'Initial desktop configure/build'};free_before_bytes=(Get-PSDrive C).Free;minimum_free_bytes=$minimum})
        cmake -S . -B $build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBREAKWATER_BUILD_DESKTOP=ON "-DBREAKWATER_ENABLE_ASAN=$(if ($Mode -eq 'asan') {'ON'} else {'OFF'})"
        if ($LASTEXITCODE -ne 0) { throw "Configure failed: $LASTEXITCODE" }
        if ($Incremental) {
            cmake --build $build --target breakwater breakwater_scene_framing_tests breakwater_label_placement_tests -- -n
            if ($LASTEXITCODE -ne 0) { throw 'Incremental dry-run failed.' }
        }
        cmake --build $build --parallel 2 --target breakwater breakwater_scene_framing_tests breakwater_label_placement_tests
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
    } *> $log
} finally { Get-Content -LiteralPath $log -Tail 22 }
