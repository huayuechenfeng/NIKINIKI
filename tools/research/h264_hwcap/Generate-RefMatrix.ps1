[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [ValidateRange(1, 60)]
    [int]$DurationSeconds = 10,
    [string]$Ffmpeg,
    [string]$Ffprobe,
    [string]$Python,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\ref-matrix'
}
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
$toolPath = Join-Path $PSScriptRoot 'h264_matrix.py'
$manifestPath = Join-Path $outputPath 'manifest.json'

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
        throw "$Name was not found. Pass -$($Name.Substring(0,1).ToUpperInvariant() + $Name.Substring(1))."
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

function Invoke-NativeToFile(
    [string]$Executable,
    [string[]]$Arguments,
    [string]$Destination
) {
    $output = @(& $Executable @Arguments 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    [IO.File]::WriteAllLines($Destination, $output, [Text.UTF8Encoding]::new($false))
    if ($exitCode -ne 0) {
        throw "Command failed with exit code $exitCode. See $Destination"
    }
}

function Invoke-PythonTool([string[]]$Arguments, [string]$LogPath) {
    Invoke-LoggedNative -Executable $Python -Arguments (@($toolPath) + $Arguments) -LogPath $LogPath |
        Out-Null
}

function Get-RecordedArguments([string[]]$Arguments) {
    $normalizedRoot = $outputPath.TrimEnd('\', '/')
    return @($Arguments | ForEach-Object {
        $_.Replace($normalizedRoot, '${OUTPUT}')
    })
}

function Get-X264ReferenceUsage([string]$LogContent, [int]$ExpectedReferences) {
    $match = [regex]::Match(
        $LogContent,
        'ref P L0:\s+(?<values>(?:[0-9]+(?:\.[0-9]+)?%\s*)+)',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) {
        if ($ExpectedReferences -eq 1) {
            return [PSCustomObject]@{
                Percentages = @(100.0)
                HighestIndexUsed = 0
                HighestDeclaredSlotExercised = $true
            }
        }
        return [PSCustomObject]@{
            Percentages = @()
            HighestIndexUsed = -1
            HighestDeclaredSlotExercised = $false
        }
    }
    $percentages = @([regex]::Matches($match.Groups['values'].Value, '[0-9]+(?:\.[0-9]+)?') |
        ForEach-Object { [double]::Parse($_.Value, [Globalization.CultureInfo]::InvariantCulture) })
    $highest = -1
    for ($index = 0; $index -lt $percentages.Count; ++$index) {
        if ($percentages[$index] -gt 0.0) {
            $highest = $index
        }
    }
    return [PSCustomObject]@{
        Percentages = $percentages
        HighestIndexUsed = $highest
        HighestDeclaredSlotExercised = ($highest -ge $ExpectedReferences - 1)
    }
}

function Get-TraceHeaderField([string]$LogContent, [string]$FieldName) {
    $pattern = '(?m)\s' + [regex]::Escape($FieldName) + '\s+[^\r\n]*=\s*(?<value>\d+)\s*$'
    $match = [regex]::Match($LogContent, $pattern)
    if (-not $match.Success) {
        throw "FFmpeg trace_headers did not report $FieldName"
    }
    return [int]$match.Groups['value'].Value
}

$Ffmpeg = Resolve-Tool $Ffmpeg 'ffmpeg'
$Ffprobe = Resolve-Tool $Ffprobe 'ffprobe'
$Python = Resolve-Tool $Python 'python'

if (Test-Path -LiteralPath $outputPath) {
    if (-not (Test-Path -LiteralPath $outputPath -PathType Container)) {
        throw "Output path exists but is not a directory: $outputPath"
    }
    $existingOutput = Get-ChildItem -LiteralPath $outputPath -Force | Select-Object -First 1
    if ($existingOutput -and -not $Force) {
        throw "Output directory is not empty. Use -Force to overwrite named generated files: $outputPath"
    }
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $outputPath 'logs') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $outputPath 'metadata') -Force | Out-Null

$width = 640
$height = 360
$frameRate = 30
$frameCount = $DurationSeconds * $frameRate
$frameBytes = $width * $height * 3 / 2
$sourcePath = Join-Path $outputPath 'base_640x360_30.yuv'
$sourceLog = Join-Path $outputPath 'logs\base-source.log'
$sourceArguments = @(
    '-hide_banner', '-y',
    '-f', 'lavfi',
    '-i', "testsrc2=size=${width}x${height}:rate=${frameRate}:duration=${DurationSeconds}",
    '-frames:v', $frameCount.ToString(),
    '-an', '-pix_fmt', 'yuv420p', '-f', 'rawvideo',
    $sourcePath
)
Invoke-LoggedNative $Ffmpeg $sourceArguments $sourceLog | Out-Null
$sourceInfo = Get-Item -LiteralPath $sourcePath
$expectedSourceBytes = [int64]$frameBytes * $frameCount
if ($sourceInfo.Length -ne $expectedSourceBytes) {
    throw "Raw source size mismatch: expected $expectedSourceBytes, got $($sourceInfo.Length)"
}

$ffmpegVersion = (& $Ffmpeg -hide_banner -version 2>&1 | Select-Object -First 1).ToString()
$ffprobeVersion = (& $Ffprobe -hide_banner -version 2>&1 | Select-Object -First 1).ToString()
$pythonVersion = (& $Python --version 2>&1 | Select-Object -First 1).ToString()
$cases = New-Object System.Collections.Generic.List[object]
$x264Build = $null

foreach ($referenceCount in 1..8) {
    $caseId = 'REF{0}' -f $referenceCount
    Write-Host "Generating $caseId"
    $h264Path = Join-Path $outputPath "$caseId.h264"
    $mp4Path = Join-Path $outputPath "$caseId.mp4"
    $encodeLog = Join-Path $outputPath "logs\$caseId-encode.log"
    $remuxLog = Join-Path $outputPath "logs\$caseId-remux.log"
    $inspectPath = Join-Path $outputPath "metadata\$caseId-inspect.json"
    $inspectLog = Join-Path $outputPath "logs\$caseId-inspect.log"
    $traceLog = Join-Path $outputPath "logs\$caseId-trace-headers.log"
    $h264GoldenPath = Join-Path $outputPath "metadata\$caseId-h264-golden.json"
    $h264GoldenLog = Join-Path $outputPath "logs\$caseId-h264-golden.log"
    $mp4GoldenPath = Join-Path $outputPath "metadata\$caseId-mp4-golden.json"
    $mp4GoldenLog = Join-Path $outputPath "logs\$caseId-mp4-golden.log"
    $ffprobePath = Join-Path $outputPath "metadata\$caseId-ffprobe.json"

    $x264Parameters = @(
        'aud=1',
        "ref=$referenceCount",
        'bframes=0',
        'b-pyramid=none',
        'weightp=0',
        'weightb=0',
        'keyint=60',
        'min-keyint=60',
        'scenecut=0',
        'open-gop=0',
        '8x8dct=1',
        'nal-hrd=none',
        'force-cfr=1',
        'threads=1'
    ) -join ':'
    $encodeArguments = @(
        '-hide_banner', '-y', '-loglevel', 'info',
        '-f', 'rawvideo', '-pix_fmt', 'yuv420p',
        '-video_size', "${width}x${height}", '-framerate', $frameRate.ToString(),
        '-i', $sourcePath,
        '-frames:v', $frameCount.ToString(), '-an',
        '-c:v', 'libx264', '-preset', 'slow',
        '-profile:v', 'high', '-level:v', '3.0', '-pix_fmt', 'yuv420p',
        '-b:v', '900k', '-maxrate', '1200k', '-bufsize', '2400k',
        '-g', '60', '-keyint_min', '60', '-sc_threshold', '0',
        '-refs', $referenceCount.ToString(), '-bf', '0',
        '-x264-params', $x264Parameters,
        '-f', 'h264', $h264Path
    )
    Invoke-LoggedNative $Ffmpeg $encodeArguments $encodeLog | Out-Null
    $encodeLogContent = [IO.File]::ReadAllText($encodeLog)
    if (-not $x264Build) {
        $buildMatch = [regex]::Match(
            $encodeLogContent,
            'encoder\s*:\s*(?<build>[^\r\n]*libx264[^\r\n]*)',
            [Text.RegularExpressions.RegexOptions]::IgnoreCase)
        if ($buildMatch.Success) {
            $x264Build = $buildMatch.Groups['build'].Value.Trim()
        }
    }

    $remuxArguments = @(
        '-hide_banner', '-y', '-loglevel', 'info',
        '-framerate', $frameRate.ToString(), '-i', $h264Path,
        '-map', '0:v:0', '-c:v', 'copy', '-an', '-movflags', '+faststart',
        $mp4Path
    )
    Invoke-LoggedNative $Ffmpeg $remuxArguments $remuxLog | Out-Null

    Invoke-PythonTool @('inspect', $h264Path, '--output', $inspectPath) $inspectLog
    $traceArguments = @(
        '-hide_banner', '-loglevel', 'info', '-i', $h264Path,
        '-map', '0:v:0', '-c:v', 'copy', '-bsf:v', 'trace_headers',
        '-f', 'null', '-'
    )
    $traceOutput = Invoke-LoggedNative $Ffmpeg $traceArguments $traceLog
    $traceContent = $traceOutput -join [Environment]::NewLine
    Invoke-PythonTool @(
        'golden', $h264Path, '--ffmpeg', $Ffmpeg,
        '--width', $width.ToString(), '--height', $height.ToString(),
        '--expected-frames', $frameCount.ToString(), '--output', $h264GoldenPath
    ) $h264GoldenLog
    Invoke-PythonTool @(
        'golden', $mp4Path, '--ffmpeg', $Ffmpeg,
        '--width', $width.ToString(), '--height', $height.ToString(),
        '--expected-frames', $frameCount.ToString(), '--output', $mp4GoldenPath
    ) $mp4GoldenLog
    $ffprobeArguments = @(
        '-v', 'error', '-select_streams', 'v:0',
        '-show_entries',
        'stream=index,codec_name,profile,level,width,height,pix_fmt,refs,r_frame_rate,avg_frame_rate,nb_frames:format=format_name,duration,size,bit_rate',
        '-of', 'json', $mp4Path
    )
    Invoke-NativeToFile $Ffprobe $ffprobeArguments $ffprobePath

    $inspection = Get-Content -LiteralPath $inspectPath -Raw | ConvertFrom-Json
    $h264Golden = Get-Content -LiteralPath $h264GoldenPath -Raw | ConvertFrom-Json
    $mp4Golden = Get-Content -LiteralPath $mp4GoldenPath -Raw | ConvertFrom-Json
    $probe = Get-Content -LiteralPath $ffprobePath -Raw | ConvertFrom-Json
    $stream = $inspection.stream
    $traceFields = [PSCustomObject]@{
        profile_idc = Get-TraceHeaderField $traceContent 'profile_idc'
        level_idc = Get-TraceHeaderField $traceContent 'level_idc'
        max_num_ref_frames = Get-TraceHeaderField $traceContent 'max_num_ref_frames'
        max_num_reorder_frames = Get-TraceHeaderField $traceContent 'max_num_reorder_frames'
        max_dec_frame_buffering = Get-TraceHeaderField $traceContent 'max_dec_frame_buffering'
        weighted_pred_flag = Get-TraceHeaderField $traceContent 'weighted_pred_flag'
        weighted_bipred_idc = Get-TraceHeaderField $traceContent 'weighted_bipred_idc'
    }
    if ($stream.profile_idc -ne 100 -or $stream.level_idc -ne 30) {
        throw "$caseId profile/level mismatch: $($stream.profile_idc)/$($stream.level_idc)"
    }
    if ($stream.width -ne $width -or $stream.height -ne $height) {
        throw "$caseId dimensions mismatch: $($stream.width)x$($stream.height)"
    }
    if ($stream.max_num_ref_frames -ne $referenceCount) {
        throw "$caseId SPS refs mismatch: expected $referenceCount, got $($stream.max_num_ref_frames)"
    }
    if ($null -ne $stream.max_num_reorder_frames -and $stream.max_num_reorder_frames -ne 0) {
        throw "$caseId reorder mismatch: expected 0, got $($stream.max_num_reorder_frames)"
    }
    if ($null -ne $stream.max_dec_frame_buffering -and
        $stream.max_dec_frame_buffering -lt $referenceCount) {
        throw "$caseId declares DPB smaller than refs"
    }
    if ($stream.weighted_pred_flag -ne 0 -or $stream.weighted_bipred_idc -ne 0) {
        throw "$caseId weighted prediction is not disabled"
    }
    foreach ($field in @(
        'profile_idc',
        'level_idc',
        'max_num_ref_frames',
        'max_num_reorder_frames',
        'max_dec_frame_buffering',
        'weighted_pred_flag',
        'weighted_bipred_idc'
    )) {
        if ($traceFields.$field -ne $stream.$field) {
            throw "$caseId independent parser and FFmpeg trace_headers disagree on $field"
        }
    }
    if ($inspection.aud_count -ne $frameCount -or
        $inspection.access_unit_count -ne $frameCount) {
        throw "$caseId access-unit/AUD count mismatch"
    }
    $bSliceCount = 0
    if ($inspection.slice_type_counts.PSObject.Properties.Name -contains 'B') {
        $bSliceCount = [int]$inspection.slice_type_counts.B
    }
    if ($bSliceCount -ne 0) {
        throw "$caseId unexpectedly contains B slices"
    }
    if ($h264Golden.aggregate_sha256 -ne $mp4Golden.aggregate_sha256 -or
        $h264Golden.frame_count -ne $mp4Golden.frame_count) {
        throw "$caseId raw H.264 and MP4 golden decode differ"
    }
    if (-not $probe.streams -or $probe.streams[0].codec_name -ne 'h264') {
        throw "$caseId ffprobe did not report H.264"
    }

    $referenceUsage = Get-X264ReferenceUsage $encodeLogContent $referenceCount
    $caseWarnings = New-Object System.Collections.Generic.List[string]
    if (-not $referenceUsage.HighestDeclaredSlotExercised) {
        $caseWarnings.Add(
            'Highest declared reference slot was not observed in x264 summary; runtime use is not proven.')
    }
    if ($null -eq $stream.max_dec_frame_buffering) {
        $caseWarnings.Add('SPS VUI does not declare max_dec_frame_buffering.')
    }

    $cases.Add([PSCustomObject]@{
        id = $caseId
        group = 'R'
        expected = [PSCustomObject]@{
            max_num_ref_frames = $referenceCount
            max_num_reorder_frames = 0
            weighted_pred_flag = 0
            weighted_bipred_idc = 0
            bframes = 0
            b_pyramid = 'none'
        }
        parsed = $stream
        trace_headers = $traceFields
        access_units = $inspection.access_unit_count
        slice_type_counts = $inspection.slice_type_counts
        h264 = [PSCustomObject]@{
            file = [IO.Path]::GetFileName($h264Path)
            size = (Get-Item -LiteralPath $h264Path).Length
            sha256 = (Get-FileHash -LiteralPath $h264Path -Algorithm SHA256).Hash
        }
        mp4 = [PSCustomObject]@{
            file = [IO.Path]::GetFileName($mp4Path)
            size = (Get-Item -LiteralPath $mp4Path).Length
            sha256 = (Get-FileHash -LiteralPath $mp4Path -Algorithm SHA256).Hash
        }
        golden = [PSCustomObject]@{
            frame_count = $h264Golden.frame_count
            aggregate_sha256 = $h264Golden.aggregate_sha256
            h264_metadata = "metadata/$caseId-h264-golden.json"
            mp4_metadata = "metadata/$caseId-mp4-golden.json"
        }
        reference_usage = $referenceUsage
        runtime_highest_ref_exercised = $referenceUsage.HighestDeclaredSlotExercised
        header_admission_eligible = $true
        runtime_decode_eligible = $referenceUsage.HighestDeclaredSlotExercised
        logs = [PSCustomObject]@{
            encode = "logs/$caseId-encode.log"
            remux = "logs/$caseId-remux.log"
            inspect = "metadata/$caseId-inspect.json"
            trace_headers = "logs/$caseId-trace-headers.log"
            ffprobe = "metadata/$caseId-ffprobe.json"
        }
        commands = [PSCustomObject]@{
            encode = Get-RecordedArguments $encodeArguments
            remux = Get-RecordedArguments $remuxArguments
        }
        warnings = @($caseWarnings)
    })
}

$caseArray = [object[]]($cases | ForEach-Object { $_ })
$runtimeReady = @($caseArray | Where-Object runtime_decode_eligible).Count
$warningCount = @($caseArray | ForEach-Object { $_.warnings } | Where-Object { $_ }).Count
$sourceHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
$manifest = [PSCustomObject]@{
    schema_version = 1
    matrix = 'H264_REF_1_TO_8'
    generated_utc = [DateTime]::UtcNow.ToString('o')
    generator = 'tools/research/h264_hwcap/Generate-RefMatrix.ps1'
    tools = [PSCustomObject]@{
        ffmpeg = $ffmpegVersion
        ffprobe = $ffprobeVersion
        libx264 = $x264Build
        python = $pythonVersion
    }
    fixed = [PSCustomObject]@{
        width = $width
        height = $height
        fps = $frameRate
        duration_seconds = $DurationSeconds
        frame_count = $frameCount
        pixel_format = 'yuv420p'
        profile_idc = 100
        level_idc = 30
        keyint = 60
        min_keyint = 60
        scenecut = 0
        bitrate_kbps = 900
        vbv_maxrate_kbps = 1200
        vbv_bufsize_kbps = 2400
        weighted_pred_flag = 0
        weighted_bipred_idc = 0
        bframes = 0
        b_pyramid = 'none'
    }
    source = [PSCustomObject]@{
        file = [IO.Path]::GetFileName($sourcePath)
        size = $sourceInfo.Length
        sha256 = $sourceHash
        ffmpeg_filter = "testsrc2=size=${width}x${height}:rate=${frameRate}:duration=${DurationSeconds}"
        command = Get-RecordedArguments $sourceArguments
    }
    cases = $caseArray
    validation = [PSCustomObject]@{
        field_constraints_passed = $true
        independent_parser_vs_trace_headers_passed = $true
        raw_h264_vs_mp4_golden_passed = $true
        header_admission_case_count = $caseArray.Count
        runtime_decode_case_count = $runtimeReady
        warning_count = $warningCount
    }
}
$manifestJson = $manifest | ConvertTo-Json -Depth 12
[IO.File]::WriteAllText(
    $manifestPath,
    $manifestJson + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$ini = [Text.StringBuilder]::new()
[void]$ini.AppendLine('[General]')
[void]$ini.AppendLine('schema=1')
[void]$ini.AppendLine('matrix=H264_REF_1_TO_8')
[void]$ini.AppendLine("count=$($cases.Count)")
[void]$ini.AppendLine("width=$width")
[void]$ini.AppendLine("height=$height")
[void]$ini.AppendLine("fps=$frameRate")
[void]$ini.AppendLine("frames=$frameCount")
foreach ($case in $caseArray) {
    [void]$ini.AppendLine()
    [void]$ini.AppendLine("[$($case.id)]")
    [void]$ini.AppendLine("h264=$($case.h264.file)")
    [void]$ini.AppendLine("mp4=$($case.mp4.file)")
    [void]$ini.AppendLine("h264_size=$($case.h264.size)")
    [void]$ini.AppendLine("mp4_size=$($case.mp4.size)")
    [void]$ini.AppendLine("h264_sha256=$($case.h264.sha256)")
    [void]$ini.AppendLine("mp4_sha256=$($case.mp4.sha256)")
    [void]$ini.AppendLine("profile=$($case.parsed.profile_idc)")
    [void]$ini.AppendLine("level=$($case.parsed.level_idc)")
    [void]$ini.AppendLine("refs=$($case.parsed.max_num_ref_frames)")
    [void]$ini.AppendLine("dpb=$($case.parsed.max_dec_frame_buffering)")
    [void]$ini.AppendLine("reorder=$($case.parsed.max_num_reorder_frames)")
    [void]$ini.AppendLine("weightp=$($case.parsed.weighted_pred_flag)")
    [void]$ini.AppendLine("weightb=$($case.parsed.weighted_bipred_idc)")
    [void]$ini.AppendLine("access_units=$($case.access_units)")
    [void]$ini.AppendLine("golden=metadata/$($case.id)-h264-golden.json")
    [void]$ini.AppendLine("golden_sha256=$($case.golden.aggregate_sha256)")
    [void]$ini.AppendLine("runtime_decode_eligible=$($case.runtime_decode_eligible.ToString().ToLowerInvariant())")
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'cases.ini'),
    $ini.ToString(),
    [Text.UTF8Encoding]::new($false))

Write-Host "Generated REF1-REF8 matrix: $outputPath" -ForegroundColor Green
Write-Host "Header-admission cases: $($caseArray.Count)"
Write-Host "Runtime high-ref graph cases: $runtimeReady"
Write-Host "Manifest: $manifestPath"
