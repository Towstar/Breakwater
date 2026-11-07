param([string]$QtPrefix = '', [switch]$Headless, [switch]$Build)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $QtPrefix -and (Test-Path -LiteralPath "$projectRoot\.tools\Qt\6.12.0\msvc2022_64")) {
    $QtPrefix = "$projectRoot\.tools\Qt\6.12.0\msvc2022_64"
}
$desktop = if ($Headless) { 'OFF' } else { 'ON' }
$arguments = @('-S', $projectRoot, '-B', "$projectRoot\out\visual-studio", '-G', 'Visual Studio 18 2026', '-A', 'x64',
    "-DBREAKWATER_BUILD_DESKTOP=$desktop", '-DBUILD_TESTING=ON')
if ($QtPrefix) { $arguments += "-DCMAKE_PREFIX_PATH=$QtPrefix" }
& cmake @arguments
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio project generation failed.' }
# Preserve CMake's complete dependency graph, while making the solution available at repo root.
$generatedSolution = "$projectRoot\out\visual-studio\Breakwater.slnx"
if (Test-Path -LiteralPath $generatedSolution) {
    [xml]$solutionXml = Get-Content -Raw -LiteralPath $generatedSolution
    foreach ($node in $solutionXml.SelectNodes('//Project[@Path]')) {
        $node.SetAttribute('Path', 'out/visual-studio/' + $node.GetAttribute('Path'))
    }
    foreach ($node in $solutionXml.SelectNodes('//BuildDependency[@Project]')) {
        $node.SetAttribute('Project', 'out/visual-studio/' + $node.GetAttribute('Project'))
    }
    $documentation = $solutionXml.CreateElement('Folder')
    $documentation.SetAttribute('Name', '/Documentation/')
    foreach ($relativeFile in @('README.md', 'docs/IMPLEMENTATION_PLAN.md', 'docs/STATUS.md', 'AGENTS.md')) {
        $fileNode = $solutionXml.CreateElement('File')
        $fileNode.SetAttribute('Path', $relativeFile)
        [void]$documentation.AppendChild($fileNode)
    }
    [void]$solutionXml.Solution.AppendChild($documentation)
    $solutionXml.Save("$projectRoot\Breakwater.slnx")
}
if ($Build) {
    & cmake --build "$projectRoot\out\visual-studio" --config RelWithDebInfo --parallel
    if ($LASTEXITCODE -ne 0) { throw 'MSVC build failed.' }
    & ctest --test-dir "$projectRoot\out\visual-studio" -C RelWithDebInfo --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'MSVC tests failed.' }
}
Get-ChildItem -LiteralPath "$projectRoot\out\visual-studio" -File | Where-Object { $_.Extension -in '.sln', '.slnx' } | Select-Object -ExpandProperty FullName
Write-Output "Root solution: $projectRoot\Breakwater.slnx (regenerate without -Headless for the desktop project)."
