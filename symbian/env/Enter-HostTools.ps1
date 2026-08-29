[CmdletBinding()]
param(
    [string]$ToolRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $ToolRoot) {
    $ToolRoot = Join-Path $repoRoot '.tools\python'
}

$cmakeExe = Join-Path $ToolRoot 'cmake\data\bin\cmake.exe'
$ninjaExe = Join-Path $ToolRoot 'bin\ninja.exe'

foreach ($tool in @($cmakeExe, $ninjaExe)) {
    if (-not (Test-Path -LiteralPath $tool)) {
        throw "Host tool is missing: $tool. Run tools\Bootstrap-HostTools.ps1 first."
    }
}

$toolDirs = @(
    (Split-Path -Parent $cmakeExe),
    (Split-Path -Parent $ninjaExe)
) | Select-Object -Unique

$env:PATH = (($toolDirs -join [IO.Path]::PathSeparator) + [IO.Path]::PathSeparator + $env:PATH)

Write-Host 'Host tools added to the current PowerShell process:'
& $cmakeExe --version | Select-Object -First 1
Write-Host "ninja $(& $ninjaExe --version)"

if ($MyInvocation.InvocationName -ne '.') {
    Write-Warning 'Run this script with dot-sourcing to keep PATH changes: . .\symbian\env\Enter-HostTools.ps1'
}
