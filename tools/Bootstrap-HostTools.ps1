[CmdletBinding()]
param(
    [string]$Python,
    [string]$Destination,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $Destination) {
    $Destination = Join-Path $repoRoot '.tools\python'
}

if (-not $Python) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($pythonCommand) {
        $Python = $pythonCommand.Source
    }
}

if (-not $Python -or -not (Test-Path -LiteralPath $Python)) {
    throw 'Python was not found. Pass -Python with the path to python.exe.'
}

$cmakeExe = Join-Path $Destination 'cmake\data\bin\cmake.exe'
$ninjaExe = Join-Path $Destination 'bin\ninja.exe'

if ($Force -or -not (Test-Path -LiteralPath $cmakeExe) -or -not (Test-Path -LiteralPath $ninjaExe)) {
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    & $Python -m pip install --disable-pip-version-check --no-warn-script-location `
        --target $Destination 'cmake==3.31.6' 'ninja==1.11.1.4'
    if ($LASTEXITCODE -ne 0) {
        throw "Host tool installation failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path -LiteralPath $cmakeExe)) {
    throw "CMake executable was not found after installation: $cmakeExe"
}
if (-not (Test-Path -LiteralPath $ninjaExe)) {
    throw "Ninja executable was not found after installation: $ninjaExe"
}

Write-Host "CMake: $cmakeExe"
$cmakeVersion = & $cmakeExe --version
if (-not $?) {
    throw 'CMake verification failed.'
}
$cmakeVersion | Select-Object -First 1
Write-Host "Ninja: $ninjaExe"
& $ninjaExe --version
if (-not $?) {
    throw 'Ninja verification failed.'
}
