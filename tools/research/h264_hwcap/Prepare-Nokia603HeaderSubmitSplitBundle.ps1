[CmdletBinding()]
param(
    [string]$BoundaryMatrixDirectory,
    [string]$FakeMatrixDirectory,
    [string]$SisPath,
    [string]$OutputDirectory,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $BoundaryMatrixDirectory) {
    $BoundaryMatrixDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\boundary-r2-matrix'
}
if (-not $FakeMatrixDirectory) {
    $FakeMatrixDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\sps-fake-ab-r3'
}
if (-not $SisPath) {
    $SisPath = Join-Path $repositoryRoot (
        'symbian\out\devvideo-capability-release\' +
        'nikiniki_devvideo_capability_probe.sis')
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot (
        '.tmp\h264-hwcap\nokia603-header-submit-split-r4-bundle')
}

$boundaryPath = [IO.Path]::GetFullPath($BoundaryMatrixDirectory)
$fakePath = [IO.Path]::GetFullPath($FakeMatrixDirectory)
$sisFullPath = [IO.Path]::GetFullPath($SisPath)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$boundaryManifestPath = Join-Path $boundaryPath 'manifest.json'
$fakeManifestPath = Join-Path $fakePath 'manifest.json'
foreach ($required in @($boundaryManifestPath, $fakeManifestPath, $sisFullPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required R4 input is missing: $required"
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

$boundary = Get-Content -LiteralPath $boundaryManifestPath -Raw | ConvertFrom-Json
$fake = Get-Content -LiteralPath $fakeManifestPath -Raw | ConvertFrom-Json
if ($boundary.schema_version -ne 2 -or $boundary.matrix -ne 'H264_BOUNDARY_R2' -or
    -not $boundary.validation.field_constraints_passed -or
    -not $boundary.validation.raw_h264_vs_mp4_golden_passed -or
    $boundary.validation.warning_count -ne 0) {
    throw 'Boundary R2 source has not passed its legal-stream gates.'
}
if ($fake.schema_version -ne 3 -or $fake.matrix -ne 'H264_SPS_FAKE_AB_R3' -or
    -not $fake.validation.original_legal_pc_decode_passed -or
    -not $fake.validation.original_highest_ref_exercised -or
    -not $fake.validation.all_sps_occurrences_rewritten -or
    -not $fake.validation.only_max_num_ref_frames_semantic_changed -or
    -not $fake.validation.non_sps_byte_identity_passed -or
    -not $fake.validation.fake_missing_reference_evidence_passed -or
    $fake.validation.warning_count -ne 0) {
    throw 'SPS-fake R3 source has not passed its diagnostic isolation gates.'
}

$r6 = $boundary.cases | Where-Object id -eq 'R6' | Select-Object -First 1
$original = $fake.cases | Where-Object id -eq 'ORIGINAL_R7' | Select-Object -First 1
$fakeRef = $fake.cases | Where-Object id -eq 'FAKE_REF3' | Select-Object -First 1
if (-not $r6 -or -not $original -or -not $fakeRef) {
    throw 'R4 source manifests do not contain R6, ORIGINAL_R7 and FAKE_REF3.'
}

$phoneDirectory = Join-Path $outputPath 'Data\NIKINIKI\hwcap'
$installDirectory = Join-Path $outputPath 'INSTALL'
$referenceDirectory = Join-Path $outputPath 'PC_REFERENCE'
$referenceMetadataDirectory = Join-Path $referenceDirectory 'metadata'
$referenceLogsDirectory = Join-Path $referenceDirectory 'logs'
foreach ($directory in @(
    $phoneDirectory,
    $installDirectory,
    $referenceDirectory,
    $referenceMetadataDirectory,
    $referenceLogsDirectory
)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

function Copy-VerifiedStream {
    param(
        [Parameter(Mandatory = $true)][string]$SourceRoot,
        [Parameter(Mandatory = $true)]$Case
    )
    $source = Join-Path $SourceRoot $Case.h264.file
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "R4 stream is missing: $source"
    }
    $hash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    if ((Get-Item -LiteralPath $source).Length -ne $Case.h264.size -or
        $hash -ne $Case.h264.sha256) {
        throw "R4 stream does not match its source manifest: $($Case.id)"
    }
    $destination = Join-Path $phoneDirectory $Case.h264.file
    Copy-Item -LiteralPath $source -Destination $destination -Force
    if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $hash) {
        throw "R4 copied stream hash mismatch: $($Case.id)"
    }
}

Copy-VerifiedStream -SourceRoot $boundaryPath -Case $r6
Copy-VerifiedStream -SourceRoot $fakePath -Case $original
Copy-VerifiedStream -SourceRoot $fakePath -Case $fakeRef

$sisDestination = Join-Path $installDirectory 'nikiniki_devvideo_capability_probe.sis'
Copy-Item -LiteralPath $sisFullPath -Destination $sisDestination -Force

$caseOrder = @(
    'R6_NATIVE',
    'R7_NATIVE',
    'FAKE_HEADER_ORIGINAL_R7',
    'FAKE_HEADER_FAKE_REF3'
)
$cases = @(
    [PSCustomObject]@{
        id = 'R6_NATIVE'; group = 'NATIVE_R6'; legal_stream = $true; diagnostic_only = $false
        h264 = $r6.h264; admission = $r6.h264; refs = 6; header_refs = 6; dpb = 6
        runtime_decode_eligible = $true; golden = $r6.golden; pc_fake_decode = $null
    },
    [PSCustomObject]@{
        id = 'R7_NATIVE'; group = 'NATIVE_R7'; legal_stream = $true; diagnostic_only = $false
        h264 = $original.h264; admission = $original.h264; refs = 7; header_refs = 7; dpb = 7
        runtime_decode_eligible = $true; golden = $original.golden; pc_fake_decode = $null
    },
    [PSCustomObject]@{
        id = 'FAKE_HEADER_ORIGINAL_R7'; group = 'SPLIT_R7'; legal_stream = $true; diagnostic_only = $true
        h264 = $original.h264; admission = $fakeRef.h264; refs = 7; header_refs = 3; dpb = 7
        runtime_decode_eligible = $true; golden = $original.golden; pc_fake_decode = $null
    },
    [PSCustomObject]@{
        id = 'FAKE_HEADER_FAKE_REF3'; group = 'FAKE_CONTROL'; legal_stream = $false; diagnostic_only = $true
        h264 = $fakeRef.h264; admission = $fakeRef.h264; refs = 3; header_refs = 3; dpb = 7
        runtime_decode_eligible = $false; golden = $fakeRef.golden; pc_fake_decode = $fakeRef.pc_fake_decode
    }
)

$ini = New-Object Text.StringBuilder
[void]$ini.AppendLine('[General]')
[void]$ini.AppendLine('schema=4')
[void]$ini.AppendLine('matrix=H264_HEADER_SUBMIT_SPLIT_R4')
[void]$ini.AppendLine('count=4')
[void]$ini.AppendLine('order=' + ($caseOrder -join ','))
for ($index = 0; $index -lt $caseOrder.Count; ++$index) {
    [void]$ini.AppendLine("case$index=$($caseOrder[$index])")
}
foreach ($case in $cases) {
    [void]$ini.AppendLine('')
    [void]$ini.AppendLine("[$($case.id)]")
    [void]$ini.AppendLine("group=$($case.group)")
    [void]$ini.AppendLine("h264=$($case.h264.file)")
    [void]$ini.AppendLine("h264_size=$($case.h264.size)")
    [void]$ini.AppendLine("h264_sha256=$($case.h264.sha256)")
    [void]$ini.AppendLine("header_h264=$($case.admission.file)")
    [void]$ini.AppendLine("header_h264_size=$($case.admission.size)")
    [void]$ini.AppendLine("header_h264_sha256=$($case.admission.sha256)")
    [void]$ini.AppendLine("header_refs=$($case.header_refs)")
    [void]$ini.AppendLine('profile=100')
    [void]$ini.AppendLine('level=30')
    [void]$ini.AppendLine("refs=$($case.refs)")
    [void]$ini.AppendLine("dpb=$($case.dpb)")
    [void]$ini.AppendLine('reorder=0')
    [void]$ini.AppendLine('weightp=0')
    [void]$ini.AppendLine('weightb=0')
    [void]$ini.AppendLine('access_units=100')
    [void]$ini.AppendLine('eos_nal=1')
    [void]$ini.AppendLine('runtime_decode_eligible=' +
        ($(if ($case.runtime_decode_eligible) { 'true' } else { 'false' })))
    [void]$ini.AppendLine("golden_sha256=$($case.golden.aggregate_sha256)")
}
[IO.File]::WriteAllText(
    (Join-Path $phoneDirectory 'cases.ini'),
    $ini.ToString(),
    [Text.UTF8Encoding]::new($false))

$r6GoldenSource = Join-Path $boundaryPath $r6.golden.h264_metadata
$originalGoldenSource = Join-Path $fakePath $original.golden.h264_metadata
$fakeDecodeSource = Join-Path $fakePath $fakeRef.pc_fake_decode.metadata
foreach ($required in @($r6GoldenSource, $originalGoldenSource, $fakeDecodeSource)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "R4 reference metadata is missing: $required"
    }
}
Copy-Item -LiteralPath $r6GoldenSource -Destination (
    Join-Path $referenceMetadataDirectory 'R6-h264-golden.json') -Force
Copy-Item -LiteralPath $originalGoldenSource -Destination (
    Join-Path $referenceMetadataDirectory 'ORIGINAL_R7-reference-golden.json') -Force
Copy-Item -LiteralPath $fakeDecodeSource -Destination (
    Join-Path $referenceMetadataDirectory 'FAKE_REF3-pc-decode.json') -Force

foreach ($relative in @(
    'metadata\FAKE_REF3-mutation.json',
    'metadata\ORIGINAL_R7-inspect.json',
    'metadata\FAKE_REF3-inspect.json',
    'logs\FAKE_REF3-pc-decode-errors.log'
)) {
    $source = Join-Path $fakePath $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "R4 diagnostic evidence is missing: $source"
    }
    Copy-Item -LiteralPath $source -Destination (
        Join-Path $referenceDirectory $relative) -Force
}

$manifestCases = @()
foreach ($case in $cases) {
    $goldenPath = if ($case.id -eq 'R6_NATIVE') {
        'metadata/R6-h264-golden.json'
    } else {
        'metadata/ORIGINAL_R7-reference-golden.json'
    }
    $entry = [ordered]@{
        id = $case.id
        group = $case.group
        legal_stream = $case.legal_stream
        diagnostic_only = $case.diagnostic_only
        h264 = $case.h264
        admission = [ordered]@{
            h264 = $case.admission
            max_num_ref_frames = $case.header_refs
        }
        parsed = [ordered]@{
            width = 640; height = 360; profile_idc = 100; level_idc = 30
            max_num_ref_frames = $case.refs
            max_dec_frame_buffering = $case.dpb
            max_num_reorder_frames = 0
            weighted_pred_flag = 0
            weighted_bipred_idc = 0
        }
        access_units = 100
        runtime_decode_eligible = $case.runtime_decode_eligible
        golden = [ordered]@{
            aggregate_sha256 = $case.golden.aggregate_sha256
            h264_metadata = $goldenPath
        }
    }
    if ($case.pc_fake_decode) {
        $entry.pc_fake_decode = [ordered]@{
            aggregate_sha256 = $case.pc_fake_decode.aggregate_sha256
            metadata = 'metadata/FAKE_REF3-pc-decode.json'
            missing_reference_picture = $true
        }
    }
    $manifestCases += [PSCustomObject]$entry
}

$matrixManifest = [ordered]@{
    schema_version = 4
    matrix = 'H264_HEADER_SUBMIT_SPLIT_R4'
    purpose = 'Separate GetHeaderInformationL admission SPS from runtime submitted SPS/AUs.'
    fixed = [ordered]@{ width = 640; height = 360; fps = 30; decoder_uid = '0x10204C21' }
    case_order = $caseOrder
    cases = $manifestCases
    evidence = [ordered]@{
        boundary_manifest_sha256 = (Get-FileHash -LiteralPath $boundaryManifestPath -Algorithm SHA256).Hash
        fake_manifest_sha256 = (Get-FileHash -LiteralPath $fakeManifestPath -Algorithm SHA256).Hash
        warning = 'Split/fake cases are diagnostic admission probes, not a playback solution or H1 proof.'
    }
}
[IO.File]::WriteAllText(
    (Join-Path $referenceDirectory 'manifest.json'),
    ($matrixManifest | ConvertTo-Json -Depth 12) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$bundleManifest = [ordered]@{
    schema_version = 4
    purpose = 'NIKINIKI_NOKIA603_DIRECT_DEVVIDEO_HEADER_SUBMIT_SPLIT_R4'
    probe = [ordered]@{
        file = 'INSTALL/nikiniki_devvideo_capability_probe.sis'
        uid = '0xE000B11D'
        decoder_uid = '0x10204C21'
        size = (Get-Item -LiteralPath $sisDestination).Length
        sha256 = (Get-FileHash -LiteralPath $sisDestination -Algorithm SHA256).Hash
    }
    matrix = [ordered]@{
        name = 'H264_HEADER_SUBMIT_SPLIT_R4'
        case_order = $caseOrder
        pc_reference_manifest_sha256 = (Get-FileHash -LiteralPath (
            Join-Path $referenceDirectory 'manifest.json') -Algorithm SHA256).Hash
    }
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'bundle-manifest.json'),
    ($bundleManifest | ConvertTo-Json -Depth 10) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$instructions = @'
NIKINIKI Nokia 603 Direct DevVideo Header/Submit split R4 probe

This is a research-only diagnostic. It does not modify ROM or NIKINIKI.
The split case admits with FAKE_REF3 only for GetHeaderInformationL/Configure,
then submits the untouched legal ORIGINAL_R7 access units after Initialize.

1. Fully exit NIKINIKI, the system video player, and other video applications.
2. Copy this bundle's Data directory to the memory-card root and merge it:
   F:\Data\NIKINIKI\hwcap\cases.ini
3. Uninstall the old NIKINIKI H264 HwCap Probe, then install the SIS in INSTALL.
4. Launch NIKINIKI H264 HwCap Probe and tap Run capability matrix.
5. Wait for all four cases, then copy the newest complete directory from:
   F:\Data\NIKINIKI\hwcap\results\

Expected order:
R6_NATIVE -> R7_NATIVE -> FAKE_HEADER_ORIGINAL_R7 -> FAKE_HEADER_FAKE_REF3

Interpretation is deferred until PC CRC comparison. Header success, AU write
success, or damaged pictures do not alone prove physical VideoCore handoff.
'@
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'RUN_ON_NOKIA603_R4.txt'),
    $instructions,
    [Text.UTF8Encoding]::new($false))

Write-Host "Nokia 603 Header/Submit split R4 bundle prepared: $outputPath" -ForegroundColor Green
Write-Host "Probe SIS SHA-256: $($bundleManifest.probe.sha256)"
Write-Host "Cases: $($caseOrder -join ' -> ')"
