param(
    [string]$QtPrefix = '',
    [string]$PythonExecutable = '',
    [switch]$Headless,
    [switch]$Build,
    [ValidateSet('Visual Studio 18 2026', 'Visual Studio 17 2022')]
    [string]$Generator = 'Visual Studio 18 2026',
    [switch]$EmbeddedDebugInfo
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $QtPrefix -and (Test-Path -LiteralPath "$projectRoot\.tools\Qt\6.12.0\msvc2022_64")) {
    $QtPrefix = "$projectRoot\.tools\Qt\6.12.0\msvc2022_64"
}
$desktop = if ($Headless) { 'OFF' } else { 'ON' }
$arguments = @('-S', $projectRoot, '-B', "$projectRoot\out\visual-studio", '-G', $Generator, '-A', 'x64',
    "-DBREAKWATER_BUILD_DESKTOP=$desktop", '-DBUILD_TESTING=ON')
if ($EmbeddedDebugInfo) { $arguments += '-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded' }
if ($QtPrefix) { $arguments += "-DCMAKE_PREFIX_PATH=$QtPrefix" }
if (-not $PythonExecutable) { $PythonExecutable = $env:BREAKWATER_PYTHON }
if ($PythonExecutable) { $arguments += "-DPython3_EXECUTABLE=$PythonExecutable" }
& cmake @arguments
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio project generation failed.' }
# Preserve CMake's complete dependency graph, while making the solution available at repo root.
$generatedSolution = "$projectRoot\out\visual-studio\Breakwater.slnx"
$legacySolution = "$projectRoot\out\visual-studio\Breakwater.sln"
if ($Generator -eq 'Visual Studio 17 2022' -and (Test-Path -LiteralPath $legacySolution)) {
    if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
        throw 'Converting the generated .sln requires a .NET SDK with dotnet sln migrate support.'
    }
    if (Test-Path -LiteralPath $generatedSolution) { Remove-Item -LiteralPath $generatedSolution }
    & dotnet sln $legacySolution migrate
    if ($LASTEXITCODE -ne 0) { throw 'Converting the generated Visual Studio solution to .slnx failed.' }
}
if (-not (Test-Path -LiteralPath $generatedSolution)) {
    throw "CMake did not produce the expected solution: $generatedSolution"
}
if (Test-Path -LiteralPath $generatedSolution) {
    [xml]$solutionXml = Get-Content -Raw -LiteralPath $generatedSolution
    $startupProject = if ($Headless) { 'engine/breakwater_cli.vcxproj' } else { 'desktop/breakwater.vcxproj' }
    foreach ($node in $solutionXml.SelectNodes('//Project[@Path]')) {
        $relativeProjectPath = $node.GetAttribute('Path').Replace('\', '/')
        $node.SetAttribute('Path', 'out/visual-studio/' + $relativeProjectPath)
        $node.RemoveAttribute('DefaultStartup')
        if ($relativeProjectPath -eq $startupProject) { $node.SetAttribute('DefaultStartup', 'true') }
    }
    foreach ($node in $solutionXml.SelectNodes('//BuildDependency[@Project]')) {
        $node.SetAttribute('Project', 'out/visual-studio/' + $node.GetAttribute('Project').Replace('\', '/'))
    }
    $documentation = $solutionXml.CreateElement('Folder')
    $documentation.SetAttribute('Name', '/Documentation/')
    foreach ($relativeFile in @('README.md', 'docs/IMPLEMENTATION_PLAN.md', 'docs/STATUS.md', 'AGENTS.md')) {
        $fileNode = $solutionXml.CreateElement('File')
        $fileNode.SetAttribute('Path', $relativeFile)
        [void]$documentation.AppendChild($fileNode)
    }
    [void]$solutionXml.Solution.AppendChild($documentation)
    $writerSettings = [System.Xml.XmlWriterSettings]::new()
    $writerSettings.Indent = $true
    $writerSettings.NewLineChars = "`n"
    $writerSettings.Encoding = [System.Text.UTF8Encoding]::new($false)
    $solutionWriter = [System.Xml.XmlWriter]::Create("$projectRoot\Breakwater.slnx", $writerSettings)
    try { $solutionXml.Save($solutionWriter) }
    finally { $solutionWriter.Dispose() }
}
if ($Build) {
    & cmake --build "$projectRoot\out\visual-studio" --config RelWithDebInfo --parallel
    if ($LASTEXITCODE -ne 0) { throw 'MSVC build failed.' }
    & ctest --test-dir "$projectRoot\out\visual-studio" -C RelWithDebInfo --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'MSVC tests failed.' }
}
Get-ChildItem -LiteralPath "$projectRoot\out\visual-studio" -File | Where-Object { $_.Extension -in '.sln', '.slnx' } | Select-Object -ExpandProperty FullName
Write-Output "Root solution: $projectRoot\Breakwater.slnx (regenerate without -Headless for the desktop project)."
