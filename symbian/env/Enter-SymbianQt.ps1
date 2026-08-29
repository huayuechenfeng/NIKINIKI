[CmdletBinding()]
param(
    [string]$EnvironmentBatch,
    [string[]]$SearchRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$detectScript = Join-Path $PSScriptRoot 'Detect-SymbianQt.ps1'

if (-not $EnvironmentBatch) {
    $detectionJson = & $detectScript -SearchRoot $SearchRoot -Json
    $detection = $detectionJson | ConvertFrom-Json
    $EnvironmentBatch = $detection.EnvironmentBatch
}

if (-not $EnvironmentBatch -or -not (Test-Path -LiteralPath $EnvironmentBatch)) {
    throw @'
No Qt for Symbian environment batch file was found.
Install the legacy Qt/Symbian SDK to a short ASCII path such as C:\QtSDK, then run:
  . .\symbian\env\Enter-SymbianQt.ps1 -SearchRoot C:\QtSDK
or pass its qtenv2.bat explicitly with -EnvironmentBatch.
'@
}

$resolvedBatch = (Resolve-Path -LiteralPath $EnvironmentBatch).Path
$batchCommand = 'call "' + $resolvedBatch + '" >nul 2>&1 && set'
$environmentLines = & $env:ComSpec /d /s /c $batchCommand
if ($LASTEXITCODE -ne 0) {
    throw "The SDK environment batch failed: $resolvedBatch"
}

$importedNames = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($line in $environmentLines) {
    $separator = $line.IndexOf('=')
    if ($separator -le 0) {
        continue
    }
    $name = $line.Substring(0, $separator)
    $value = $line.Substring($separator + 1)
    if (-not $name.StartsWith('=') -and $importedNames.Add($name)) {
        # Update PowerShell's Env: provider as well as the native process
        # environment so Get-Command can immediately resolve SDK tools. The
        # Codex host can expose both PATH and Path; cmd.exe preserves both, so
        # keep the first (SDK-modified) value case-insensitively.
        Set-Item -Path "Env:$name" -Value $value
    }
}

Write-Host "Imported Symbian Qt environment from: $resolvedBatch"
& $detectScript -SearchRoot $SearchRoot -RequireReady

if ($MyInvocation.InvocationName -ne '.') {
    Write-Warning 'Run this script with dot-sourcing to keep environment changes: . .\symbian\env\Enter-SymbianQt.ps1'
}
