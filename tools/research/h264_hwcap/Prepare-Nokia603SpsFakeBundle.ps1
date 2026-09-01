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
    $MatrixDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\sps-fake-ab-r3'
}
if (-not $SisPath) {
    $SisPath = Join-Path $repositoryRoot (
        'symbian\out\devvideo-capability-release\' +
        'nikiniki_devvideo_capability_probe.sis')
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot (
        '.tmp\h264-hwcap\nokia603-sps-fake-ab-r3-bundle')
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
        throw "Output directory is not empty. Use -Force to overwrite named files: $outputPath"
    }
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
if ($manifest.schema_version -ne 3 -or
    $manifest.matrix -ne 'H264_SPS_FAKE_AB_R3' -or
    $manifest.cases.Count -ne 2 -or
    $manifest.case_order.Count -ne 2 -or
    -not $manifest.validation.original_legal_pc_decode_passed -or
    -not $manifest.validation.original_highest_ref_exercised -or
    -not $manifest.validation.all_sps_occurrences_rewritten -or
    -not $manifest.validation.only_max_num_ref_frames_semantic_changed -or
    -not $manifest.validation.non_sps_byte_identity_passed -or
    -not $manifest.validation.pps_byte_identity_passed -or
    -not $manifest.validation.access_unit_and_nal_structure_identity_passed -or
    -not $manifest.validation.independent_parser_vs_trace_headers_passed -or
    -not $manifest.validation.fake_missing_reference_evidence_passed -or
    -not $manifest.validation.fake_decode_differs_from_legal_golden -or
    -not $manifest.validation.fake_is_not_legal_capability_evidence -or
    $manifest.validation.warning_count -ne 0) {
    throw 'SPS-fake A/B has not passed every required diagnostic isolation gate.'
}
if (($manifest.case_order -join ',') -ne 'ORIGINAL_R7,FAKE_REF3') {
    throw 'Unexpected SPS-fake A/B case order.'
}

$installDirectory = Join-Path $outputPath 'INSTALL'
$phoneDirectory = Join-Path $outputPath 'Data\NIKINIKI\hwcap'
$referenceDirectory = Join-Path $outputPath 'PC_REFERENCE'
$referenceMetadataDirectory = Join-Path $referenceDirectory 'metadata'
$referenceLogsDirectory = Join-Path $referenceDirectory 'logs'
foreach ($directory in @(
    $installDirectory,
    $phoneDirectory,
    $referenceDirectory,
    $referenceMetadataDirectory,
    $referenceLogsDirectory
)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$sisDestination = Join-Path $installDirectory 'nikiniki_devvideo_capability_probe.sis'
Copy-Item -LiteralPath $sisFullPath -Destination $sisDestination -Force
Copy-Item -LiteralPath $casesPath -Destination (Join-Path $phoneDirectory 'cases.ini') -Force
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $referenceDirectory 'manifest.json') -Force

$referenceFiles = @(
    'metadata\FAKE_REF3-mutation.json',
    'metadata\ORIGINAL_R7-inspect.json',
    'metadata\FAKE_REF3-inspect.json',
    'metadata\ORIGINAL_R7-reference-golden.json',
    'metadata\FAKE_REF3-pc-decode.json',
    'logs\FAKE_REF3-pc-decode-errors.log',
    'logs\ORIGINAL_R7-trace-headers.log',
    'logs\FAKE_REF3-trace-headers.log'
)
foreach ($relative in $referenceFiles) {
    $source = Join-Path $matrixPath $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "A/B reference file is missing: $source"
    }
    $destination = Join-Path $referenceDirectory $relative
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

$bundleCases = @()
foreach ($caseId in $manifest.case_order) {
    $case = $manifest.cases | Where-Object id -eq $caseId | Select-Object -First 1
    if (-not $case) {
        throw "Manifest case_order references a missing case: $caseId"
    }
    $source = Join-Path $matrixPath $case.h264.file
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "A/B stream is missing: $source"
    }
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ((Get-Item -LiteralPath $source).Length -ne $case.h264.size -or
        $sourceHash -ne $case.h264.sha256) {
        throw "A/B stream does not match manifest: $caseId"
    }
    $destination = Join-Path $phoneDirectory $case.h264.file
    Copy-Item -LiteralPath $source -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $sourceHash) {
        throw "Copied A/B stream hash mismatch: $caseId"
    }
    $bundleCases += [PSCustomObject]@{
        id = $caseId
        group = $case.group
        legal_stream = $case.legal_stream
        diagnostic_only = $case.diagnostic_only
        file = "Data/NIKINIKI/hwcap/$($case.h264.file)"
        size = $case.h264.size
        sha256 = $sourceHash
        refs = $case.parsed.max_num_ref_frames
        dpb = $case.parsed.max_dec_frame_buffering
    }
}

$bundleManifest = [PSCustomObject]@{
    schema_version = 3
    purpose = 'NIKINIKI_NOKIA603_DIRECT_DEVVIDEO_SPS_FAKE_AB_R3'
    warning = 'FAKE_REF3 is intentionally inconsistent and is not legal capability evidence.'
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
    mutation = $manifest.mutation
    cases = $bundleCases
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'bundle-manifest.json'),
    ($bundleManifest | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$instructions = @'
NIKINIKI Nokia 603 Direct DevVideo original-vs-all-SPS-fake R3 probe

WARNING: FAKE_REF3 is deliberately inconsistent. It is a diagnostic input,
not a playable conversion and not evidence that ref=7 has been unlocked.

1. Fully exit NIKINIKI, the system video player, and other video applications.
2. Copy this bundle's Data directory to the phone memory card root and merge it.
   Nokia 603 path: F:\Data\NIKINIKI\hwcap\cases.ini
3. Uninstall the old NIKINIKI H264 HwCap Probe, then install the SIS in INSTALL.
4. Launch NIKINIKI H264 HwCap Probe and tap Run capability matrix.
5. Wait for both cases, then copy the newest complete directory from:
   F:\Data\NIKINIKI\hwcap\results\

Expected order:
ORIGINAL_R7 -> FAKE_REF3

ORIGINAL_R7 is the legal ref=7 control. FAKE_REF3 has every SPS
max_num_ref_frames changed from 7 to 3; PPS and every non-SPS NAL are unchanged.
Do not open FAKE_REF3 in the formal NIKINIKI application or treat its pictures
as valid playback output.
'@
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'RUN_ON_NOKIA603_R3.txt'),
    $instructions,
    [Text.UTF8Encoding]::new($false))

Write-Host "Nokia 603 SPS-fake A/B R3 bundle prepared: $outputPath" -ForegroundColor Green
Write-Host "Probe SIS SHA-256: $($bundleManifest.probe.sha256)"
Write-Host 'Cases: ORIGINAL_R7 (legal) -> FAKE_REF3 (diagnostic-only)'
