[CmdletBinding()]
param(
    [string]$SourceMatrixDirectory,
    [string]$OutputDirectory,
    [string]$Ffmpeg,
    [string]$Python,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $SourceMatrixDirectory) {
    $SourceMatrixDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\boundary-r2-matrix'
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\sps-fake-ab-r3'
}
$sourceMatrixPath = [IO.Path]::GetFullPath($SourceMatrixDirectory)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$toolPath = Join-Path $PSScriptRoot 'h264_matrix.py'

function Resolve-Tool([string]$Requested, [string]$Name) {
    if ($Requested) {
        if (Test-Path -LiteralPath $Requested -PathType Leaf) {
            return (Resolve-Path -LiteralPath $Requested).Path
        }
        $requestedCommand = Get-Command $Requested -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($requestedCommand) {
            return $requestedCommand.Source
        }
        throw "$Name was not found: $Requested"
    }
    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $command) {
        throw "$Name was not found. Pass an explicit -$Name path."
    }
    return $command.Source
}

function Invoke-LoggedNative(
    [string]$Executable,
    [string[]]$Arguments,
    [string]$LogPath
) {
    $output = @(& $Executable @Arguments 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    [IO.File]::WriteAllLines($LogPath, $output, [Text.UTF8Encoding]::new($false))
    if ($exitCode -ne 0) {
        throw "Command failed with exit code $exitCode. See $LogPath"
    }
    return $output
}

function Invoke-PythonTool([string[]]$Arguments, [string]$LogPath) {
    Invoke-LoggedNative -Executable $Python -Arguments (@($toolPath) + $Arguments) `
        -LogPath $LogPath | Out-Null
}

function Get-TraceHeaderField([string]$LogContent, [string]$FieldName) {
    $pattern = '(?m)\s' + [regex]::Escape($FieldName) + '\s+[^\r\n]*=\s*(?<value>\d+)\s*$'
    $match = [regex]::Match($LogContent, $pattern)
    if (-not $match.Success) {
        throw "FFmpeg trace_headers did not report $FieldName"
    }
    return [int]$match.Groups['value'].Value
}

function ConvertTo-StableJson($Value) {
    return ($Value | ConvertTo-Json -Depth 12 -Compress)
}

$Ffmpeg = Resolve-Tool $Ffmpeg 'ffmpeg'
$Python = Resolve-Tool $Python 'python'
$sourceManifestPath = Join-Path $sourceMatrixPath 'manifest.json'
if (-not (Test-Path -LiteralPath $sourceManifestPath -PathType Leaf)) {
    throw "Source boundary manifest is missing: $sourceManifestPath"
}
$sourceManifest = Get-Content -LiteralPath $sourceManifestPath -Raw | ConvertFrom-Json
$sourceCase = $sourceManifest.cases | Where-Object id -eq 'R7' | Select-Object -First 1
if ($sourceManifest.schema_version -ne 2 -or
    $sourceManifest.matrix -ne 'H264_BOUNDARY_R2' -or
    -not $sourceCase -or
    $sourceCase.parsed.max_num_ref_frames -ne 7 -or
    $sourceCase.parsed.max_dec_frame_buffering -ne 7 -or
    $sourceCase.parsed.max_num_reorder_frames -ne 0 -or
    $sourceCase.parsed.weighted_pred_flag -ne 0 -or
    -not $sourceCase.runtime_decode_eligible) {
    throw 'Source R7 has not passed the required legal boundary-matrix gates.'
}
$sourceR7Path = Join-Path $sourceMatrixPath $sourceCase.h264.file
$sourceGoldenPath = Join-Path $sourceMatrixPath $sourceCase.golden.h264_metadata
foreach ($required in @($sourceR7Path, $sourceGoldenPath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Source R7 input is missing: $required"
    }
}
if ((Get-Item -LiteralPath $sourceR7Path).Length -ne $sourceCase.h264.size -or
    (Get-FileHash -LiteralPath $sourceR7Path -Algorithm SHA256).Hash -ne $sourceCase.h264.sha256) {
    throw 'Source R7 bytes do not match the boundary manifest.'
}

if (Test-Path -LiteralPath $outputPath) {
    if (-not (Test-Path -LiteralPath $outputPath -PathType Container)) {
        throw "Output path exists but is not a directory: $outputPath"
    }
    if ((Get-ChildItem -LiteralPath $outputPath -Force | Select-Object -First 1) -and -not $Force) {
        throw "Output directory is not empty. Use -Force to overwrite named files: $outputPath"
    }
}
foreach ($directory in @($outputPath, (Join-Path $outputPath 'logs'), (Join-Path $outputPath 'metadata'))) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$originalPath = Join-Path $outputPath 'ORIGINAL_R7.h264'
$fakePath = Join-Path $outputPath 'FAKE_REF3.h264'
Copy-Item -LiteralPath $sourceR7Path -Destination $originalPath -Force

$mutationPath = Join-Path $outputPath 'metadata\FAKE_REF3-mutation.json'
Invoke-PythonTool @(
    'rewrite-sps-refs', $originalPath, $fakePath, '--value', '3', '--report', $mutationPath
) (Join-Path $outputPath 'logs\FAKE_REF3-mutation.log')

$originalInspectPath = Join-Path $outputPath 'metadata\ORIGINAL_R7-inspect.json'
$fakeInspectPath = Join-Path $outputPath 'metadata\FAKE_REF3-inspect.json'
Invoke-PythonTool @('inspect', $originalPath, '--output', $originalInspectPath) `
    (Join-Path $outputPath 'logs\ORIGINAL_R7-inspect.log')
Invoke-PythonTool @('inspect', $fakePath, '--output', $fakeInspectPath) `
    (Join-Path $outputPath 'logs\FAKE_REF3-inspect.log')

$originalTraceArguments = @(
    '-hide_banner', '-loglevel', 'info', '-i', $originalPath,
    '-map', '0:v:0', '-c:v', 'copy', '-bsf:v', 'trace_headers', '-f', 'null', '-'
)
$fakeTraceArguments = @(
    '-hide_banner', '-loglevel', 'info', '-i', $fakePath,
    '-map', '0:v:0', '-c:v', 'copy', '-bsf:v', 'trace_headers', '-f', 'null', '-'
)
$originalTraceOutput = Invoke-LoggedNative $Ffmpeg $originalTraceArguments `
    (Join-Path $outputPath 'logs\ORIGINAL_R7-trace-headers.log')
$fakeTraceOutput = Invoke-LoggedNative $Ffmpeg $fakeTraceArguments `
    (Join-Path $outputPath 'logs\FAKE_REF3-trace-headers.log')
$originalTraceContent = $originalTraceOutput -join [Environment]::NewLine
$fakeTraceContent = $fakeTraceOutput -join [Environment]::NewLine

$originalGoldenPath = Join-Path $outputPath 'metadata\ORIGINAL_R7-reference-golden.json'
$fakePcDecodePath = Join-Path $outputPath 'metadata\FAKE_REF3-pc-decode.json'
Invoke-PythonTool @(
    'golden', $originalPath, '--ffmpeg', $Ffmpeg, '--width', '640', '--height', '360',
    '--expected-frames', '100', '--output', $originalGoldenPath
) (Join-Path $outputPath 'logs\ORIGINAL_R7-golden.log')
Invoke-PythonTool @(
    'golden', $fakePath, '--ffmpeg', $Ffmpeg, '--width', '640', '--height', '360',
    '--expected-frames', '100', '--output', $fakePcDecodePath
) (Join-Path $outputPath 'logs\FAKE_REF3-pc-decode-crc.log')
$fakeDecodeOutput = Invoke-LoggedNative $Ffmpeg @(
    '-hide_banner', '-v', 'error', '-err_detect', 'explode', '-i', $fakePath,
    '-map', '0:v:0', '-f', 'null', '-'
) (Join-Path $outputPath 'logs\FAKE_REF3-pc-decode-errors.log')
$fakeDecodeText = $fakeDecodeOutput -join [Environment]::NewLine

$originalInspection = Get-Content -LiteralPath $originalInspectPath -Raw | ConvertFrom-Json
$fakeInspection = Get-Content -LiteralPath $fakeInspectPath -Raw | ConvertFrom-Json
$mutation = Get-Content -LiteralPath $mutationPath -Raw | ConvertFrom-Json
$originalGolden = Get-Content -LiteralPath $originalGoldenPath -Raw | ConvertFrom-Json
$fakePcDecode = Get-Content -LiteralPath $fakePcDecodePath -Raw | ConvertFrom-Json
$originalStream = $originalInspection.stream
$fakeStream = $fakeInspection.stream

if ($originalStream.max_num_ref_frames -ne 7 -or $fakeStream.max_num_ref_frames -ne 3 -or
    $originalStream.max_dec_frame_buffering -ne 7 -or $fakeStream.max_dec_frame_buffering -ne 7 -or
    $originalStream.max_num_reorder_frames -ne 0 -or $fakeStream.max_num_reorder_frames -ne 0 -or
    $originalStream.weighted_pred_flag -ne 0 -or $fakeStream.weighted_pred_flag -ne 0) {
    throw 'Original/fake parsed field contract failed.'
}
if ($mutation.sps_rewritten -ne $originalInspection.sps_occurrences -or
    @($mutation.old_values | Where-Object { $_ -ne 7 }).Count -ne 0 -or
    $mutation.new_value -ne 3 -or
    -not $mutation.only_max_num_ref_frames_semantic_changed -or
    -not $mutation.non_sps_identical -or
    -not $mutation.nal_type_sequence_identical -or
    -not $mutation.pps_identical -or
    -not $mutation.access_unit_count_identical) {
    throw 'All-SPS-fake mutation isolation gate failed.'
}
if ((ConvertTo-StableJson $originalInspection.pps) -ne (ConvertTo-StableJson $fakeInspection.pps) -or
    (ConvertTo-StableJson $originalInspection.slice_type_counts) -ne
        (ConvertTo-StableJson $fakeInspection.slice_type_counts) -or
    $originalInspection.access_unit_count -ne 100 -or $fakeInspection.access_unit_count -ne 100 -or
    $originalInspection.aud_count -ne 100 -or $fakeInspection.aud_count -ne 100 -or
    $originalInspection.nal_unit_counts.'11' -ne 1 -or $fakeInspection.nal_unit_counts.'11' -ne 1) {
    throw 'Original/fake non-SPS structure gate failed.'
}

$traceFields = @(
    'profile_idc', 'level_idc', 'max_num_ref_frames', 'max_num_reorder_frames',
    'max_dec_frame_buffering', 'weighted_pred_flag', 'weighted_bipred_idc',
    'num_ref_idx_l0_default_active_minus1'
)
$originalTrace = [ordered]@{}
$fakeTrace = [ordered]@{}
foreach ($field in $traceFields) {
    $originalTrace[$field] = Get-TraceHeaderField $originalTraceContent $field
    $fakeTrace[$field] = Get-TraceHeaderField $fakeTraceContent $field
}
if ($originalTrace.max_num_ref_frames -ne 7 -or $fakeTrace.max_num_ref_frames -ne 3 -or
    $originalTrace.max_dec_frame_buffering -ne 7 -or $fakeTrace.max_dec_frame_buffering -ne 7 -or
    $originalTrace.num_ref_idx_l0_default_active_minus1 -ne 6 -or
    $fakeTrace.num_ref_idx_l0_default_active_minus1 -ne 6) {
    throw 'Independent trace_headers A/B contract failed.'
}
foreach ($field in $traceFields | Where-Object { $_ -ne 'max_num_ref_frames' }) {
    if ($originalTrace[$field] -ne $fakeTrace[$field]) {
        throw "Unexpected trace_headers difference outside max_num_ref_frames: $field"
    }
}
$pcFakeFrameMatches = 0
$pcFakeFirstMismatch = -1
for ($frameIndex = 0; $frameIndex -lt $originalGolden.frames.Count; ++$frameIndex) {
    if ($originalGolden.frames[$frameIndex].crc32 -eq $fakePcDecode.frames[$frameIndex].crc32) {
        ++$pcFakeFrameMatches
    } elseif ($pcFakeFirstMismatch -lt 0) {
        $pcFakeFirstMismatch = $frameIndex
    }
}
if ($originalGolden.aggregate_sha256 -eq $fakePcDecode.aggregate_sha256 -or
    $pcFakeFirstMismatch -lt 0 -or
    $fakeDecodeText -notmatch 'Missing reference picture') {
    throw 'PC fake stream did not demonstrate the expected reference corruption.'
}

$caseOrder = @('ORIGINAL_R7', 'FAKE_REF3')
$cases = @(
    [PSCustomObject]@{
        id = 'ORIGINAL_R7'
        group = 'AB_ORIGINAL'
        diagnostic_only = $false
        legal_stream = $true
        runtime_decode_eligible = $true
        parsed = $originalStream
        pps = $originalInspection.pps[0]
        access_units = 100
        h264 = [PSCustomObject]@{
            file = 'ORIGINAL_R7.h264'
            size = (Get-Item -LiteralPath $originalPath).Length
            sha256 = (Get-FileHash -LiteralPath $originalPath -Algorithm SHA256).Hash
        }
        golden = [PSCustomObject]@{
            aggregate_sha256 = $originalGolden.aggregate_sha256
            h264_metadata = 'metadata/ORIGINAL_R7-reference-golden.json'
        }
    },
    [PSCustomObject]@{
        id = 'FAKE_REF3'
        group = 'AB_FAKE'
        diagnostic_only = $true
        legal_stream = $false
        runtime_decode_eligible = $false
        parsed = $fakeStream
        pps = $fakeInspection.pps[0]
        access_units = 100
        h264 = [PSCustomObject]@{
            file = 'FAKE_REF3.h264'
            size = (Get-Item -LiteralPath $fakePath).Length
            sha256 = (Get-FileHash -LiteralPath $fakePath -Algorithm SHA256).Hash
        }
        golden = [PSCustomObject]@{
            aggregate_sha256 = $originalGolden.aggregate_sha256
            h264_metadata = 'metadata/ORIGINAL_R7-reference-golden.json'
            comparison_basis = 'ORIGINAL_R7 legal decode; fake PC decode is intentionally corrupt'
        }
        pc_fake_decode = [PSCustomObject]@{
            aggregate_sha256 = $fakePcDecode.aggregate_sha256
            metadata = 'metadata/FAKE_REF3-pc-decode.json'
            error_log = 'logs/FAKE_REF3-pc-decode-errors.log'
            missing_reference_picture = $true
            frame_crc_matches_against_legal = $pcFakeFrameMatches
            first_crc_mismatch = $pcFakeFirstMismatch
        }
    }
)

$manifest = [PSCustomObject]@{
    schema_version = 3
    matrix = 'H264_SPS_FAKE_AB_R3'
    generated_utc = [DateTime]::UtcNow.ToString('o')
    generator = 'tools/research/h264_hwcap/Generate-SpsFakeAb.ps1'
    case_order = $caseOrder
    fixed = [PSCustomObject]@{
        width = 640
        height = 360
        fps = 30
        frame_count = 100
        pixel_format = 'yuv420p'
        profile_idc = 100
        level_idc = 30
        bframes = 0
        max_num_reorder_frames = 0
        weighted_pred_flag = 0
        weighted_bipred_idc = 0
        max_dec_frame_buffering = 7
        pps_num_ref_idx_l0_default_active_minus1 = 6
        eos_nal_type = 11
    }
    source = [PSCustomObject]@{
        matrix = $sourceManifest.matrix
        manifest_sha256 = (Get-FileHash -LiteralPath $sourceManifestPath -Algorithm SHA256).Hash
        case = 'R7'
        h264_sha256 = $sourceCase.h264.sha256
        runtime_highest_ref_exercised = $sourceCase.runtime_highest_ref_exercised
    }
    mutation = [PSCustomObject]@{
        report = 'metadata/FAKE_REF3-mutation.json'
        sps_occurrences_rewritten = $mutation.sps_rewritten
        old_max_num_ref_frames = 7
        new_max_num_ref_frames = 3
        non_sps_sha256 = $mutation.non_sps_sha256
        non_sps_identical = $mutation.non_sps_identical
        pps_identical = $mutation.pps_identical
        only_max_num_ref_frames_semantic_changed = `
            $mutation.only_max_num_ref_frames_semantic_changed
    }
    trace_headers = [PSCustomObject]@{
        original = [PSCustomObject]$originalTrace
        fake = [PSCustomObject]$fakeTrace
    }
    cases = $cases
    validation = [PSCustomObject]@{
        original_legal_pc_decode_passed = $true
        original_highest_ref_exercised = $true
        all_sps_occurrences_rewritten = $true
        only_max_num_ref_frames_semantic_changed = $true
        non_sps_byte_identity_passed = $true
        pps_byte_identity_passed = $true
        access_unit_and_nal_structure_identity_passed = $true
        independent_parser_vs_trace_headers_passed = $true
        fake_missing_reference_evidence_passed = $true
        fake_decode_differs_from_legal_golden = $true
        fake_is_not_legal_capability_evidence = $true
        warning_count = 0
    }
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'manifest.json'),
    ($manifest | ConvertTo-Json -Depth 14) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$ini = [Text.StringBuilder]::new()
[void]$ini.AppendLine('[General]')
[void]$ini.AppendLine('schema=3')
[void]$ini.AppendLine('matrix=H264_SPS_FAKE_AB_R3')
[void]$ini.AppendLine('count=2')
[void]$ini.AppendLine('order=ORIGINAL_R7,FAKE_REF3')
[void]$ini.AppendLine('case0=ORIGINAL_R7')
[void]$ini.AppendLine('case1=FAKE_REF3')
[void]$ini.AppendLine('width=640')
[void]$ini.AppendLine('height=360')
[void]$ini.AppendLine('fps=30')
[void]$ini.AppendLine('frames=100')
foreach ($case in $cases) {
    [void]$ini.AppendLine()
    [void]$ini.AppendLine("[$($case.id)]")
    [void]$ini.AppendLine("group=$($case.group)")
    [void]$ini.AppendLine("h264=$($case.h264.file)")
    [void]$ini.AppendLine("h264_size=$($case.h264.size)")
    [void]$ini.AppendLine("h264_sha256=$($case.h264.sha256)")
    [void]$ini.AppendLine("profile=$($case.parsed.profile_idc)")
    [void]$ini.AppendLine("level=$($case.parsed.level_idc)")
    [void]$ini.AppendLine("refs=$($case.parsed.max_num_ref_frames)")
    [void]$ini.AppendLine("dpb=$($case.parsed.max_dec_frame_buffering)")
    [void]$ini.AppendLine("reorder=$($case.parsed.max_num_reorder_frames)")
    [void]$ini.AppendLine("weightp=$($case.parsed.weighted_pred_flag)")
    [void]$ini.AppendLine("weightb=$($case.parsed.weighted_bipred_idc)")
    [void]$ini.AppendLine('access_units=100')
    [void]$ini.AppendLine('eos_nal=1')
    [void]$ini.AppendLine("golden_sha256=$($case.golden.aggregate_sha256)")
    [void]$ini.AppendLine("runtime_decode_eligible=$($case.runtime_decode_eligible.ToString().ToLowerInvariant())")
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'cases.ini'),
    $ini.ToString(),
    [Text.UTF8Encoding]::new($false))

Write-Host "Generated SPS-fake Direct A/B: $outputPath" -ForegroundColor Green
Write-Host "Original R7 SHA-256: $($cases[0].h264.sha256)"
Write-Host "Fake ref=3 SHA-256: $($cases[1].h264.sha256)"
Write-Host 'Diagnostic contract: all SPS changed; every non-SPS NAL byte-identical.'
