[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9-]+$')]
    [string]$Project = 'qt-minimal',
    [ValidateSet('debug', 'release')]
    [string]$Configuration = 'release',
    [string]$MakeTarget,
    [switch]$SkipSis
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sourceDirectory = Join-Path $PSScriptRoot "probes\$Project"
$outputDirectory = Join-Path $PSScriptRoot "out\$Project-$Configuration"
$projectFile = Join-Path $outputDirectory "$Project.pro"

if (-not (Test-Path -LiteralPath $sourceDirectory -PathType Container)) {
    throw "Unknown probe '$Project': $sourceDirectory does not exist."
}

$qmakeCommand = Get-Command qmake -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $qmakeCommand) {
    throw 'qmake is missing. Dot-source symbian\env\Enter-SymbianQt.ps1 first.'
}

$makeCommand = @('mingw32-make', 'make') |
    ForEach-Object { Get-Command $_ -ErrorAction SilentlyContinue | Select-Object -First 1 } |
    Where-Object { $_ } |
    Select-Object -First 1
if (-not $makeCommand) {
    throw 'No make command was found in the active Symbian SDK environment.'
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $sourceDirectory "$Project.pro") -Destination $projectFile -Force
Copy-Item -LiteralPath (Join-Path $sourceDirectory 'main.cpp') -Destination $outputDirectory -Force
Push-Location $outputDirectory
try {
    & $qmakeCommand.Source $projectFile "CONFIG+=$Configuration"
    if ($LASTEXITCODE -ne 0) {
        throw "qmake failed with exit code $LASTEXITCODE."
    }

    if (-not $MakeTarget) {
        $makefileText = Get-Content -LiteralPath (Join-Path $outputDirectory 'Makefile') -Raw
        foreach ($candidate in @("$Configuration-gcce", "$Configuration-armv5", $Configuration, 'all')) {
            if ($makefileText -match "(?m)^$([regex]::Escape($candidate))\s*:") {
                $MakeTarget = $candidate
                break
            }
        }
    }

    if (-not $MakeTarget) {
        throw 'Could not determine a Symbian make target. Pass -MakeTarget explicitly.'
    }

    $makeOverrides = @()
    $sbsBatch = Get-Command sbs.bat -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($sbsBatch -and $makefileText -match '(?m)^SBS\s*=\s*sbs\s*$') {
        # GNU make cannot launch the extensionless POSIX sbs shim on Windows.
        $makeOverrides += "SBS=$($sbsBatch.Source)"
    }

    Write-Host "Building target: $MakeTarget"
    & $makeCommand.Source @makeOverrides $MakeTarget
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    if (-not $SkipSis) {
        $makefileText = Get-Content -LiteralPath (Join-Path $outputDirectory 'Makefile') -Raw
        if ($makefileText -match '(?m)^sis\s*:') {
            & $makeCommand.Source @makeOverrides sis
            if ($LASTEXITCODE -ne 0) {
                throw "SIS packaging failed with exit code $LASTEXITCODE."
            }
        } else {
            Write-Warning 'This qmake spec did not generate a sis target. Use the SDK packaging command recorded in TOOLCHAIN_REPORT.md.'
        }
    }
} finally {
    Pop-Location
}

Write-Host "Probe output: $outputDirectory"
