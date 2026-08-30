[CmdletBinding()]
param(
    [ValidateSet('debug', 'release')]
    [string]$Configuration = 'debug',
    [ValidateSet('', 'ffmpegsoft1', 'ffmpegsoft2', 'devvideodirectprobe1')]
    [string]$Variant = '',
    [string]$MakeTarget,
    [switch]$SkipSis
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$sourceDirectory = Join-Path $PSScriptRoot 'app'
$projectFile = Join-Path $sourceDirectory 'wiliwili_symbian.pro'
$outputSuffix = if ($Variant) { "$Configuration-$Variant" } else { $Configuration }
$outputDirectory = Join-Path $PSScriptRoot "out\wiliwili-symbian-$outputSuffix"

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

# The mainline always ships the ffmpegsoft2 software fallback.  Keep the
# legacy -Variant values accepted below so old diagnostic build commands still
# work, but do not let omission of a variant silently produce an MMF-only app.
$ffmpegLibraryDirectory = Join-Path $PSScriptRoot 'third_party\ppsspp_ffmpeg\lib\gcce'
$codecLibrary = Join-Path $ffmpegLibraryDirectory 'libavcodec.lib'
$utilLibrary = Join-Path $ffmpegLibraryDirectory 'libavutil.lib'
if (-not (Test-Path -LiteralPath $codecLibrary -PathType Leaf) -or
    -not (Test-Path -LiteralPath $utilLibrary -PathType Leaf)) {
    throw 'FFmpeg software-decoder libraries are missing. Run symbian\third_party\ppsspp_ffmpeg\Build-Gcce-H264.ps1 first.'
}
$sdkRoot = Split-Path -Parent (Split-Path -Parent $qmakeCommand.Source)
foreach ($mode in @('udeb', 'urel')) {
    $sdkLibraryDirectory = Join-Path $sdkRoot "epoc32\release\armv5\$mode"
    if (-not (Test-Path -LiteralPath $sdkLibraryDirectory -PathType Container)) {
        throw "Symbian SDK library directory is missing: $sdkLibraryDirectory"
    }
    # The SDK may mark staged archives as protected/read-only.  Avoid a
    # needless overwrite when the staged bytes already match the source; this
    # keeps reproducible builds working without requiring elevated access.
    foreach ($library in @($codecLibrary, $utilLibrary)) {
        $destination = Join-Path $sdkLibraryDirectory (Split-Path -Leaf $library)
        $sameBytes = $false
        if (Test-Path -LiteralPath $destination -PathType Leaf) {
            $sameBytes = (Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash -eq
                (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        }
        if (-not $sameBytes) {
            Copy-Item -LiteralPath $library -Destination $destination -Force
        }
    }
}
Write-Host 'Staged mainline PPSSPP-FFmpeg archives in the Symbian SDK armv5 library directories.'

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
# Qt 4.7's Symbian qmake generator resolves source and qrc paths incorrectly
# when the Makefile lives outside the project directory. Build in-source and
# copy only the final package into the stable output directory.
Push-Location $sourceDirectory
try {
    $qmakeArguments = @($projectFile, "CONFIG+=$Configuration")
    if ($Variant) {
        $qmakeArguments += "CONFIG+=$Variant"
    }
    & $qmakeCommand.Source @qmakeArguments
    if ($LASTEXITCODE -ne 0) {
        throw "qmake failed with exit code $LASTEXITCODE."
    }

    $makefileText = Get-Content -LiteralPath (Join-Path $sourceDirectory 'Makefile') -Raw
    if (-not $MakeTarget) {
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

    if (-not $SkipSis -and $makefileText -match '(?m)^sis\s*:') {
        & $makeCommand.Source @makeOverrides sis
        if ($LASTEXITCODE -ne 0) {
            throw "SIS packaging failed with exit code $LASTEXITCODE."
        }

        $sourceSis = Join-Path $sourceDirectory 'wiliwili_symbian.sis'
        if (-not (Test-Path -LiteralPath $sourceSis -PathType Leaf)) {
            throw "SIS packaging completed but $sourceSis was not produced."
        }
        Copy-Item -LiteralPath $sourceSis -Destination $outputDirectory -Force
    }
} finally {
    Pop-Location
}

Write-Host "Application output: $outputDirectory"
