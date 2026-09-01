[CmdletBinding()]
param(
    [string]$MatrixDirectory,
    [string]$SisPath,
    [string]$OutputDirectory,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $MatrixDirectory) {
    $MatrixDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\ref-matrix'
}
if (-not $SisPath) {
    $SisPath = Join-Path $repositoryRoot (
        'symbian\out\devvideo-capability-release\' +
        'nikiniki_devvideo_capability_probe.sis')
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\nokia603-bundle'
}

$matrixPath = [IO.Path]::GetFullPath($MatrixDirectory)
$sisFullPath = [IO.Path]::GetFullPath($SisPath)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$manifestPath = Join-Path $matrixPath 'manifest.json'
$casesPath = Join-Path $matrixPath 'cases.ini'
foreach ($required in @($manifestPath, $casesPath, $sisFullPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required bundle input is missing: $required"
    }
}

if (Test-Path -LiteralPath $outputPath) {
    if (-not (Test-Path -LiteralPath $outputPath -PathType Container)) {
        throw "Output exists but is not a directory: $outputPath"
    }
    $existing = Get-ChildItem -LiteralPath $outputPath -Force | Select-Object -First 1
    if ($existing -and -not $Force) {
        throw "Output directory is not empty. Use -Force to overwrite named bundle files: $outputPath"
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 1 -or
    $manifest.matrix -ne 'H264_REF_1_TO_8' -or
    $manifest.cases.Count -ne 8 -or
    $manifest.validation.runtime_decode_case_count -ne 8 -or
    -not $manifest.validation.field_constraints_passed -or
    -not $manifest.validation.independent_parser_vs_trace_headers_passed -or
    -not $manifest.validation.raw_h264_vs_mp4_golden_passed) {
    throw 'Matrix manifest has not passed the required legal-stream gates.'
}

$installDirectory = Join-Path $outputPath 'INSTALL'
$phoneDirectory = Join-Path $outputPath 'Data\NIKINIKI\hwcap'
$referenceDirectory = Join-Path $outputPath 'PC_REFERENCE'
$referenceMetadataDirectory = Join-Path $referenceDirectory 'metadata'
foreach ($directory in @(
    $installDirectory,
    $phoneDirectory,
    $referenceDirectory,
    $referenceMetadataDirectory
)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$sisDestination = Join-Path $installDirectory 'nikiniki_devvideo_capability_probe.sis'
Copy-Item -LiteralPath $sisFullPath -Destination $sisDestination -Force
Copy-Item -LiteralPath $casesPath -Destination (Join-Path $phoneDirectory 'cases.ini') -Force
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $referenceDirectory 'manifest.json') -Force

$bundleCases = @()
foreach ($case in $manifest.cases) {
    $source = Join-Path $matrixPath $case.h264.file
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Matrix stream is missing: $source"
    }
    $sourceItem = Get-Item -LiteralPath $source
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ($sourceItem.Length -ne $case.h264.size -or $sourceHash -ne $case.h264.sha256) {
        throw "Matrix stream does not match manifest: $($case.id)"
    }
    $destination = Join-Path $phoneDirectory $case.h264.file
    Copy-Item -LiteralPath $source -Destination $destination -Force
    $destinationHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
    if ($destinationHash -ne $sourceHash) {
        throw "Copied stream hash mismatch: $($case.id)"
    }
    $bundleCases += [PSCustomObject]@{
        id = $case.id
        file = "Data/NIKINIKI/hwcap/$($case.h264.file)"
        size = $sourceItem.Length
        sha256 = $sourceHash
        refs = $case.parsed.max_num_ref_frames
        dpb = $case.parsed.max_dec_frame_buffering
    }
    $goldenSource = Join-Path $matrixPath $case.golden.h264_metadata
    if (-not (Test-Path -LiteralPath $goldenSource -PathType Leaf)) {
        throw "Golden frame metadata is missing: $goldenSource"
    }
    Copy-Item -LiteralPath $goldenSource -Destination (
        Join-Path $referenceMetadataDirectory "$($case.id)-h264-golden.json") -Force
}

$bundleManifest = [PSCustomObject]@{
    schema_version = 1
    purpose = 'NIKINIKI_NOKIA603_DIRECT_DEVVIDEO_H264_RESEARCH'
    probe = [PSCustomObject]@{
        file = 'INSTALL/nikiniki_devvideo_capability_probe.sis'
        uid = '0xE000B11D'
        decoder_uid = '0x10204C21'
        size = (Get-Item -LiteralPath $sisDestination).Length
        sha256 = (Get-FileHash -LiteralPath $sisDestination -Algorithm SHA256).Hash
    }
    cases = $bundleCases
}
$bundleManifestJson = $bundleManifest | ConvertTo-Json -Depth 8
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'bundle-manifest.json'),
    $bundleManifestJson + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$instructions = @'
NIKINIKI Nokia 603 Direct DevVideo capability probe

1. Fully exit NIKINIKI, the system video player, and other video applications.
2. Copy this bundle's Data directory to the phone memory card root.
   Nokia 603 path: F:\Data\NIKINIKI\hwcap\cases.ini
   E:\Data\NIKINIKI\hwcap\cases.ini and C:\Data\NIKINIKI\hwcap\cases.ini are also supported.
3. Install INSTALL\nikiniki_devvideo_capability_probe.sis.
4. Launch NIKINIKI H264 HwCap Probe and tap Run REF matrix.
5. Wait for all 8 cases, then copy the newest directory from:
   F:\Data\NIKINIKI\hwcap\results\
   (Use the same drive where cases.ini was copied.)

Do not launch the formal NIKINIKI application during the run.
'@
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'RUN_ON_NOKIA603.txt'),
    $instructions,
    [Text.UTF8Encoding]::new($false))

Write-Host "Nokia 603 bundle prepared: $outputPath" -ForegroundColor Green
Write-Host "Probe SIS SHA-256: $($bundleManifest.probe.sha256)"
Write-Host "Legal H.264 cases: $($bundleCases.Count)"
