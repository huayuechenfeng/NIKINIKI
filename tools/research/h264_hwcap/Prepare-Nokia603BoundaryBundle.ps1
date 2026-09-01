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
    $MatrixDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\boundary-r2-matrix'
}
if (-not $SisPath) {
    $SisPath = Join-Path $repositoryRoot (
        'symbian\out\devvideo-capability-release\' +
        'nikiniki_devvideo_capability_probe.sis')
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot (
        '.tmp\h264-hwcap\nokia603-boundary-r2-bundle')
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
    if ((Get-ChildItem -LiteralPath $outputPath -Force | Select-Object -First 1) -and -not $Force) {
        throw "Output directory is not empty. Use -Force to overwrite named bundle files: $outputPath"
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 2 -or
    $manifest.matrix -ne 'H264_BOUNDARY_R2' -or
    $manifest.cases.Count -ne 11 -or
    $manifest.case_order.Count -ne 11 -or
    -not $manifest.validation.field_constraints_passed -or
    -not $manifest.validation.independent_parser_vs_trace_headers_passed -or
    -not $manifest.validation.raw_h264_vs_mp4_golden_passed -or
    -not $manifest.validation.explicit_eos_passed -or
    -not $manifest.validation.dpb_rewrite_non_sps_identity_passed -or
    -not $manifest.validation.weighted_controls_byte_identity_passed -or
    $manifest.validation.warning_count -ne 0) {
    throw 'Boundary matrix has not passed every required legal-stream gate.'
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
foreach ($caseId in $manifest.case_order) {
    $case = $manifest.cases | Where-Object id -eq $caseId | Select-Object -First 1
    if (-not $case) {
        throw "Manifest case_order references a missing case: $caseId"
    }
    $source = Join-Path $matrixPath $case.h264.file
    $goldenSource = Join-Path $matrixPath $case.golden.h264_metadata
    foreach ($required in @($source, $goldenSource)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Boundary case input is missing: $required"
        }
    }
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ((Get-Item -LiteralPath $source).Length -ne $case.h264.size -or
        $sourceHash -ne $case.h264.sha256) {
        throw "Boundary stream does not match manifest: $caseId"
    }
    $destination = Join-Path $phoneDirectory $case.h264.file
    Copy-Item -LiteralPath $source -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $sourceHash) {
        throw "Copied stream hash mismatch: $caseId"
    }
    Copy-Item -LiteralPath $goldenSource -Destination (
        Join-Path $referenceMetadataDirectory "$caseId-h264-golden.json") -Force
    $bundleCases += [PSCustomObject]@{
        id = $caseId
        group = $case.group
        file = "Data/NIKINIKI/hwcap/$($case.h264.file)"
        size = $case.h264.size
        sha256 = $sourceHash
        refs = $case.parsed.max_num_ref_frames
        dpb = $case.parsed.max_dec_frame_buffering
        weightp = $case.parsed.weighted_pred_flag
        eos_nal = $case.expected.eos_nal
    }
}

$bundleManifest = [PSCustomObject]@{
    schema_version = 2
    purpose = 'NIKINIKI_NOKIA603_DIRECT_DEVVIDEO_BOUNDARY_R2'
    probe = [PSCustomObject]@{
        file = 'INSTALL/nikiniki_devvideo_capability_probe.sis'
        uid = '0xE000B11D'
        decoder_uid = '0x10204C21'
        size = (Get-Item -LiteralPath $sisDestination).Length
        sha256 = (Get-FileHash -LiteralPath $sisDestination -Algorithm SHA256).Hash
    }
    matrix = [PSCustomObject]@{
        name = $manifest.matrix
        case_order = $manifest.case_order
        manifest_sha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    }
    cases = $bundleCases
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'bundle-manifest.json'),
    ($bundleManifest | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$instructions = @'
NIKINIKI Nokia 603 Direct DevVideo boundary R2 probe

This replaces the first REF1-REF8 matrix data and probe SIS.

1. Fully exit NIKINIKI, the system video player, and other video applications.
2. Copy this bundle's Data directory to the phone memory card root and merge it.
   Nokia 603 path: F:\Data\NIKINIKI\hwcap\cases.ini
3. Uninstall the old NIKINIKI H264 HwCap Probe, then install the SIS in INSTALL.
4. Launch NIKINIKI H264 HwCap Probe and tap Run capability matrix.
5. Wait for all 11 cases, then copy the newest complete directory from:
   F:\Data\NIKINIKI\hwcap\results\

Expected order:
R6 -> R7 -> D4 -> D5 -> D6 -> D7 -> D8 ->
WP4_OFF -> WP4_ON -> WP6_OFF -> WP6_ON

Do not launch the formal NIKINIKI application during the run.
'@
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'RUN_ON_NOKIA603_R2.txt'),
    $instructions,
    [Text.UTF8Encoding]::new($false))

Write-Host "Nokia 603 boundary R2 bundle prepared: $outputPath" -ForegroundColor Green
Write-Host "Probe SIS SHA-256: $($bundleManifest.probe.sha256)"
Write-Host "Legal H.264 cases: $($bundleCases.Count)"
