param(
    [string]$LeanBin = '',
    [switch]$FetchCache
)
$ErrorActionPreference = 'Stop'
$proofRoot = $PSScriptRoot
$projectRoot = Split-Path -Parent $proofRoot
$outputDirectory = Join-Path $projectRoot 'out/a6-proofs'
# This explicit count guards the advertised list in Breakwater/Audit.lean.
$expectedAlgorithmTheorems = 54

if ($LeanBin) {
    if (-not (Test-Path -LiteralPath (Join-Path $LeanBin 'lake.exe'))) {
        throw 'LeanBin must contain the pinned Lean/Lake binaries.'
    }
    $env:PATH = "$LeanBin;$env:PATH"
}
if (-not (Get-Command lake -ErrorAction SilentlyContinue) -or
    -not (Get-Command lean -ErrorAction SilentlyContinue)) {
    throw 'Install Lean 4.35.0 or pass -LeanBin. See proofs/README.md.'
}
$env:MATHLIB_NO_CACHE_ON_UPDATE = '1'
if (-not $env:MATHLIB_CACHE_DIR) {
    $env:MATHLIB_CACHE_DIR = Join-Path $projectRoot '.tools/mathlib-cache'
}

Push-Location -LiteralPath $proofRoot
try {
    $leanVersion = & lean --version
    if ($LASTEXITCODE -ne 0 -or $leanVersion -notmatch 'version 4\.35\.0,') {
        throw "Expected pinned Lean 4.35.0, got: $leanVersion"
    }
    $lakeVersion = & lake --version
    if ($LASTEXITCODE -ne 0) { throw 'Lake version check failed.' }
    $manifest = Get-Content -Raw -LiteralPath (Join-Path $proofRoot 'lake-manifest.json') | ConvertFrom-Json
    $mathlib = @($manifest.packages | Where-Object { $_.name -eq 'mathlib' })
    if ($mathlib.Count -ne 1 -or $mathlib[0].rev -ne '56a005855ea17e0b0958f73ec801ae140b0a1679') {
        throw 'Mathlib manifest does not match the audited pin.'
    }
    $sources = @(
        Get-Item -LiteralPath (Join-Path $proofRoot 'Breakwater.lean')
        Get-ChildItem -LiteralPath (Join-Path $proofRoot 'Breakwater') -Filter '*.lean' -File
    )
    foreach ($source in $sources) {
        # Strict lexical policy in owned sources, including comments; dependency
        # assumptions are checked transitively by the default Audit module below.
        $text = Get-Content -Raw -LiteralPath $source.FullName
        if ($text -match '\b(sorry|admit|axiom|unsafe|native_decide|sorryAx|skipKernelTC)\b') {
            throw "Prohibited proof token in $($source.Name): $($Matches[1])"
        }
    }
    if ($FetchCache) {
        & lake exe cache get Mathlib.Algebra.BigOperators.Fin `
            Mathlib.Algebra.BigOperators.Group.List.Basic Mathlib.Basic.Real.Basic `
            Mathlib.Tactic.Ring Mathlib.Tactic.Linarith
        if ($LASTEXITCODE -ne 0) { throw 'Pinned partial Mathlib cache fetch failed.' }
    }
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    $start = [DateTime]::UtcNow
    $buildOutput = @(& lake build 2>&1)
    $buildExit = $LASTEXITCODE
    $buildText = ($buildOutput | ForEach-Object { "$_" }) -join "`n"
    [System.IO.File]::WriteAllText((Join-Path $outputDirectory 'lake-build.log'), $buildText + "`n")
    $buildOutput | ForEach-Object { Write-Output "$_" }
    if ($buildExit -ne 0) { throw "lake build failed. Evidence: $outputDirectory" }

    # Re-execute the build-integrated audit even when Lake reuses cached modules.
    $auditOutput = @(& lake env lean -DwarningAsError=true -DautoImplicit=false Breakwater/Audit.lean 2>&1)
    $auditExit = $LASTEXITCODE
    $auditText = ($auditOutput | ForEach-Object { "$_" }) -join "`n"
    [System.IO.File]::WriteAllText((Join-Path $outputDirectory 'axiom-audit.log'), $auditText + "`n")
    $auditOutput | ForEach-Object { Write-Output "$_" }
    if ($auditExit -ne 0 -or $auditText -notmatch "BREAKWATER assumption audit passed for $expectedAlgorithmTheorems algorithm theorems") {
        throw "Transitive assumption audit failed. Evidence: $outputDirectory"
    }
    # Exercise the same audit function with a deliberately unapproved assumption.
    # This expected-failure probe is generated only in ignored out/, never imported
    # by the checked proof library or included in its source/assumption claims.
    $negativePath = Join-Path $outputDirectory 'RejectAssumption.lean'
    $negativeSource = @'
module
meta import Breakwater.Audit
axiom A6UnexpectedProbe : False
theorem A6RejectedProbe : False := A6UnexpectedProbe
run_cmd Breakwater.auditAlgorithmTheorems #[``A6RejectedProbe]
'@
    [System.IO.File]::WriteAllText($negativePath, $negativeSource + "`n")
    $negativeOutput = @(& lake env lean -DwarningAsError=true $negativePath 2>&1)
    $negativeExit = $LASTEXITCODE
    $negativeText = ($negativeOutput | ForEach-Object { "$_" }) -join "`n"
    [System.IO.File]::WriteAllText((Join-Path $outputDirectory 'negative-audit.log'), $negativeText + "`n")
    if ($negativeExit -eq 0 -or $negativeText -notmatch 'Unapproved transitive assumption' -or
        $negativeText -notmatch 'A6UnexpectedProbe') {
        throw "Assumption rejection regression failed. Evidence: $outputDirectory"
    }
    $records = @(
        foreach ($source in $sources) {
            [ordered]@{ path = [System.IO.Path]::GetRelativePath($proofRoot, $source.FullName).Replace('\','/')
                sha256 = (Get-FileHash -LiteralPath $source.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
        }
    )
    $record = [ordered]@{
        status = 'passed'
        started_utc = $start.ToString('o')
        finished_utc = [DateTime]::UtcNow.ToString('o')
        lean_version = "$leanVersion"
        lake_version = "$lakeVersion"
        mathlib_revision = $mathlib[0].rev
        manifest_sha256 = (Get-FileHash -LiteralPath (Join-Path $proofRoot 'lake-manifest.json') -Algorithm SHA256).Hash.ToLowerInvariant()
        audited_algorithm_theorems = $expectedAlgorithmTheorems
        permitted_foundational_axioms = @('propext', 'Classical.choice', 'Quot.sound')
        warning_as_error = $true
        unapproved_assumption_probe_rejected = $true
        source_hashes = $records
        scope = 'Exact-real finite-graph storage/control-influence, discrete FVM mass balance and conditional native-law nonnegativity; not IEEE floating-point or compiled nonnegativity, compiled memory safety, PDE accuracy, scheduling, or hydraulics.'
    }
    [System.IO.File]::WriteAllText((Join-Path $outputDirectory 'verification.json'),
        ($record | ConvertTo-Json -Depth 8) + "`n")
    Write-Output "Checked algorithm proof evidence: $outputDirectory"
    # The expected-failure probe must not leak a failing native exit status.
    $global:LASTEXITCODE = 0
} finally {
    Pop-Location
}
