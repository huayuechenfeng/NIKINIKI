[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [string]$Ffmpeg,
    [string]$Ffprobe,
    [string]$Python,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot '.tmp\h264-hwcap\boundary-r2-matrix'
}
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
    return @($Arguments | ForEach-Object { $_.Replace($normalizedRoot, '${OUTPUT}') })
}

function Get-TraceHeaderField([string]$LogContent, [string]$FieldName) {
    $pattern = '(?m)\s' + [regex]::Escape($FieldName) + '\s+[^\r\n]*=\s*(?<value>\d+)\s*$'
    $match = [regex]::Match($LogContent, $pattern)
    if (-not $match.Success) {
        throw "FFmpeg trace_headers did not report $FieldName"
    }
    return [int]$match.Groups['value'].Value
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

function Get-X264WeightedPPercent([string]$LogContent) {
    $match = [regex]::Match(
        $LogContent,
        'Weighted P-Frames:\s*Y:\s*(?<value>[0-9]+(?:\.[0-9]+)?)%',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (-not $match.Success) {
        return 0.0
    }
    return [double]::Parse(
        $match.Groups['value'].Value,
        [Globalization.CultureInfo]::InvariantCulture)
}

$Ffmpeg = Resolve-Tool $Ffmpeg 'ffmpeg'
$Ffprobe = Resolve-Tool $Ffprobe 'ffprobe'
$Python = Resolve-Tool $Python 'python'

if (Test-Path -LiteralPath $outputPath) {
    if (-not (Test-Path -LiteralPath $outputPath -PathType Container)) {
        throw "Output path exists but is not a directory: $outputPath"
    }
    if ((Get-ChildItem -LiteralPath $outputPath -Force | Select-Object -First 1) -and -not $Force) {
        throw "Output directory is not empty. Use -Force to overwrite named generated files: $outputPath"
    }
}
foreach ($directory in @(
    $outputPath,
    (Join-Path $outputPath 'intermediate'),
    (Join-Path $outputPath 'logs'),
    (Join-Path $outputPath 'metadata')
)) {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
}

$width = 640
$height = 360
$frameRate = 30
$frameCount = 100
$frameBytes = $width * $height * 3 / 2
$sourceFilter = "testsrc2=size=${width}x${height}:rate=${frameRate}," +
    'fade=t=in:st=0:d=1,fade=t=out:st=2:d=1'
$sourcePath = Join-Path $outputPath 'base_640x360_30_100f.yuv'
$sourceLog = Join-Path $outputPath 'logs\base-source.log'
$sourceArguments = @(
    '-hide_banner', '-y', '-f', 'lavfi',
    '-i', $sourceFilter,
    '-frames:v', $frameCount.ToString(), '-an',
    '-pix_fmt', 'yuv420p', '-f', 'rawvideo', $sourcePath
)
Invoke-LoggedNative $Ffmpeg $sourceArguments $sourceLog | Out-Null
if ((Get-Item -LiteralPath $sourcePath).Length -ne [int64]$frameBytes * $frameCount) {
    throw 'Raw source size mismatch.'
}

$definitions = @(
    [PSCustomObject]@{ id='R6'; group='R'; refs=6; dpb=6; weightpMode=0; weightpFlag=0; mode='encode'; origin='' },
    [PSCustomObject]@{ id='R7'; group='R'; refs=7; dpb=7; weightpMode=0; weightpFlag=0; mode='encode'; origin='' },
    [PSCustomObject]@{ id='D4'; group='D'; refs=4; dpb=4; weightpMode=0; weightpFlag=0; mode='encode'; origin='' },
    [PSCustomObject]@{ id='D5'; group='D'; refs=4; dpb=5; weightpMode=0; weightpFlag=0; mode='rewrite'; origin='D4' },
    [PSCustomObject]@{ id='D6'; group='D'; refs=4; dpb=6; weightpMode=0; weightpFlag=0; mode='rewrite'; origin='D4' },
    [PSCustomObject]@{ id='D7'; group='D'; refs=4; dpb=7; weightpMode=0; weightpFlag=0; mode='rewrite'; origin='D4' },
    [PSCustomObject]@{ id='D8'; group='D'; refs=4; dpb=8; weightpMode=0; weightpFlag=0; mode='rewrite'; origin='D4' },
    [PSCustomObject]@{ id='WP4_OFF'; group='WP'; refs=4; dpb=4; weightpMode=0; weightpFlag=0; mode='encode'; origin='' },
    [PSCustomObject]@{ id='WP4_ON'; group='WP'; refs=4; dpb=4; weightpMode=2; weightpFlag=1; mode='encode'; origin='' },
    [PSCustomObject]@{ id='WP6_OFF'; group='WP'; refs=6; dpb=6; weightpMode=0; weightpFlag=0; mode='encode'; origin='' },
    [PSCustomObject]@{ id='WP6_ON'; group='WP'; refs=6; dpb=6; weightpMode=2; weightpFlag=1; mode='encode'; origin='' }
)
$caseOrder = @($definitions | ForEach-Object id)
$caseRecords = New-Object System.Collections.Generic.List[object]
$encodeLogs = @{}
$x264Build = $null

foreach ($definition in $definitions) {
    $caseId = $definition.id
    Write-Host "Generating $caseId"
    $h264Path = Join-Path $outputPath "$caseId.h264"
    $mp4Path = Join-Path $outputPath "$caseId.mp4"
    $encodeLog = Join-Path $outputPath "logs\$caseId-encode.log"
    $mutationReportPath = Join-Path $outputPath "metadata\$caseId-mutation.json"
    $mutationLog = Join-Path $outputPath "logs\$caseId-mutation.log"
    $recordedEncode = @()
    $recordedMutation = @()

    if ($definition.mode -eq 'encode') {
        $encodedPath = Join-Path $outputPath "intermediate\$caseId-no-eos.h264"
        $x264Parameters = @(
            'aud=1',
            "ref=$($definition.refs)",
            'bframes=0',
            'b-pyramid=none',
            "weightp=$($definition.weightpMode)",
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
            '-i', $sourcePath, '-frames:v', $frameCount.ToString(), '-an',
            '-c:v', 'libx264', '-preset', 'slow', '-profile:v', 'high',
            '-level:v', '3.0', '-pix_fmt', 'yuv420p',
            '-b:v', '900k', '-maxrate', '1200k', '-bufsize', '2400k',
            '-g', '60', '-keyint_min', '60', '-sc_threshold', '0',
            '-refs', $definition.refs.ToString(), '-bf', '0',
            '-x264-params', $x264Parameters, '-f', 'h264', $encodedPath
        )
        Invoke-LoggedNative $Ffmpeg $encodeArguments $encodeLog | Out-Null
        $appendArguments = @(
            'append-eos', $encodedPath, $h264Path, '--report', $mutationReportPath
        )
        Invoke-PythonTool $appendArguments $mutationLog
        $recordedEncode = Get-RecordedArguments $encodeArguments
        $recordedMutation = Get-RecordedArguments $appendArguments
        $encodeLogs[$caseId] = $encodeLog
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
    } else {
        $originPath = Join-Path $outputPath "$($definition.origin).h264"
        if (-not (Test-Path -LiteralPath $originPath -PathType Leaf)) {
            throw "$caseId origin is missing: $originPath"
        }
        $rewriteArguments = @(
            'rewrite-sps-dpb', $originPath, $h264Path,
            '--value', $definition.dpb.ToString(), '--report', $mutationReportPath
        )
        Invoke-PythonTool $rewriteArguments $mutationLog
        $recordedMutation = Get-RecordedArguments $rewriteArguments
        $encodeLog = $encodeLogs[$definition.origin]
        $encodeLogContent = [IO.File]::ReadAllText($encodeLog)
    }

    $remuxLog = Join-Path $outputPath "logs\$caseId-remux.log"
    $remuxArguments = @(
        '-hide_banner', '-y', '-loglevel', 'info', '-framerate', $frameRate.ToString(),
        '-i', $h264Path, '-map', '0:v:0', '-c:v', 'copy', '-an',
        '-movflags', '+faststart', $mp4Path
    )
    Invoke-LoggedNative $Ffmpeg $remuxArguments $remuxLog | Out-Null

    $inspectPath = Join-Path $outputPath "metadata\$caseId-inspect.json"
    $inspectLog = Join-Path $outputPath "logs\$caseId-inspect.log"
    Invoke-PythonTool @('inspect', $h264Path, '--output', $inspectPath) $inspectLog
    $traceLog = Join-Path $outputPath "logs\$caseId-trace-headers.log"
    $traceArguments = @(
        '-hide_banner', '-loglevel', 'info', '-i', $h264Path,
        '-map', '0:v:0', '-c:v', 'copy', '-bsf:v', 'trace_headers', '-f', 'null', '-'
    )
    $traceOutput = Invoke-LoggedNative $Ffmpeg $traceArguments $traceLog
    $traceContent = $traceOutput -join [Environment]::NewLine

    $h264GoldenPath = Join-Path $outputPath "metadata\$caseId-h264-golden.json"
    $h264GoldenLog = Join-Path $outputPath "logs\$caseId-h264-golden.log"
    Invoke-PythonTool @(
        'golden', $h264Path, '--ffmpeg', $Ffmpeg,
        '--width', $width.ToString(), '--height', $height.ToString(),
        '--expected-frames', $frameCount.ToString(), '--output', $h264GoldenPath
    ) $h264GoldenLog
    $mp4GoldenPath = Join-Path $outputPath "metadata\$caseId-mp4-golden.json"
    $mp4GoldenLog = Join-Path $outputPath "logs\$caseId-mp4-golden.log"
    Invoke-PythonTool @(
        'golden', $mp4Path, '--ffmpeg', $Ffmpeg,
        '--width', $width.ToString(), '--height', $height.ToString(),
        '--expected-frames', $frameCount.ToString(), '--output', $mp4GoldenPath
    ) $mp4GoldenLog

    $ffprobePath = Join-Path $outputPath "metadata\$caseId-ffprobe.json"
    Invoke-NativeToFile $Ffprobe @(
        '-v', 'error', '-select_streams', 'v:0',
        '-show_entries',
        'stream=index,codec_name,profile,level,width,height,pix_fmt,refs,r_frame_rate,avg_frame_rate,nb_frames:format=format_name,duration,size,bit_rate',
        '-of', 'json', $mp4Path
    ) $ffprobePath

    $inspection = Get-Content -LiteralPath $inspectPath -Raw | ConvertFrom-Json
    $stream = $inspection.stream
    $h264Golden = Get-Content -LiteralPath $h264GoldenPath -Raw | ConvertFrom-Json
    $mp4Golden = Get-Content -LiteralPath $mp4GoldenPath -Raw | ConvertFrom-Json
    $mutation = Get-Content -LiteralPath $mutationReportPath -Raw | ConvertFrom-Json
    $traceFields = [PSCustomObject]@{
        profile_idc = Get-TraceHeaderField $traceContent 'profile_idc'
        level_idc = Get-TraceHeaderField $traceContent 'level_idc'
        max_num_ref_frames = Get-TraceHeaderField $traceContent 'max_num_ref_frames'
        max_num_reorder_frames = Get-TraceHeaderField $traceContent 'max_num_reorder_frames'
        max_dec_frame_buffering = Get-TraceHeaderField $traceContent 'max_dec_frame_buffering'
        weighted_pred_flag = Get-TraceHeaderField $traceContent 'weighted_pred_flag'
        weighted_bipred_idc = Get-TraceHeaderField $traceContent 'weighted_bipred_idc'
    }
    if ($stream.profile_idc -ne 100 -or $stream.level_idc -ne 30 -or
        $stream.width -ne $width -or $stream.height -ne $height -or
        $stream.max_num_ref_frames -ne $definition.refs -or
        $stream.max_dec_frame_buffering -ne $definition.dpb -or
        $stream.max_num_reorder_frames -ne 0 -or
        $stream.weighted_pred_flag -ne $definition.weightpFlag -or
        $stream.weighted_bipred_idc -ne 0) {
        throw "$caseId parsed field tuple does not match its definition."
    }
    foreach ($field in @(
        'profile_idc', 'level_idc', 'max_num_ref_frames',
        'max_num_reorder_frames', 'max_dec_frame_buffering',
        'weighted_pred_flag', 'weighted_bipred_idc'
    )) {
        if ($traceFields.$field -ne $stream.$field) {
            throw "$caseId parser and trace_headers disagree on $field"
        }
    }
    $eosCount = 0
    if ($inspection.nal_unit_counts.PSObject.Properties.Name -contains '11') {
        $eosCount = [int]$inspection.nal_unit_counts.'11'
    }
    if ($inspection.aud_count -ne $frameCount -or
        $inspection.access_unit_count -ne $frameCount -or $eosCount -ne 1) {
        throw "$caseId AUD/access-unit/EOS count mismatch."
    }
    if (($inspection.slice_type_counts.PSObject.Properties.Name -contains 'B') -or
        $h264Golden.frame_count -ne $frameCount -or
        $h264Golden.aggregate_sha256 -ne $mp4Golden.aggregate_sha256) {
        throw "$caseId decode/golden or B-slice gate failed."
    }
    if ($definition.mode -eq 'rewrite' -and
        (-not $mutation.non_sps_identical -or
         @($mutation.old_values | Where-Object { $_ -ne 4 }).Count -ne 0 -or
         $mutation.new_value -ne $definition.dpb)) {
        throw "$caseId SPS-only mutation contract failed."
    }

    $referenceUsage = Get-X264ReferenceUsage $encodeLogContent $definition.refs
    $weightedPercent = Get-X264WeightedPPercent $encodeLogContent
    if (-not $referenceUsage.HighestDeclaredSlotExercised) {
        throw "$caseId did not exercise its highest declared reference slot."
    }
    if ($definition.weightpFlag -eq 1 -and $weightedPercent -le 0.0) {
        throw "$caseId enabled weighted P but x264 reported no weighted P frames."
    }

    $caseRecords.Add([PSCustomObject]@{
        id = $caseId
        group = $definition.group
        expected = [PSCustomObject]@{
            max_num_ref_frames = $definition.refs
            max_dec_frame_buffering = $definition.dpb
            max_num_reorder_frames = 0
            weighted_pred_flag = $definition.weightpFlag
            weighted_bipred_idc = 0
            bframes = 0
            b_pyramid = 'none'
            eos_nal = 1
        }
        parsed = $stream
        trace_headers = $traceFields
        access_units = $inspection.access_unit_count
        nal_unit_counts = $inspection.nal_unit_counts
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
        weighted_p_frames_percent = $weightedPercent
        runtime_highest_ref_exercised = $true
        header_admission_eligible = $true
        runtime_decode_eligible = $true
        mutation = [PSCustomObject]@{
            mode = $definition.mode
            origin = $definition.origin
            report = "metadata/$caseId-mutation.json"
        }
        logs = [PSCustomObject]@{
            encode = if ($definition.mode -eq 'encode') { "logs/$caseId-encode.log" } else { "logs/$($definition.origin)-encode.log" }
            mutation = "logs/$caseId-mutation.log"
            remux = "logs/$caseId-remux.log"
            inspect = "metadata/$caseId-inspect.json"
            trace_headers = "logs/$caseId-trace-headers.log"
            ffprobe = "metadata/$caseId-ffprobe.json"
        }
        commands = [PSCustomObject]@{
            encode = $recordedEncode
            mutation = $recordedMutation
            remux = Get-RecordedArguments $remuxArguments
        }
        warnings = @()
    })
}

$caseArray = [object[]]($caseRecords | ForEach-Object { $_ })
$caseById = @{}
foreach ($case in $caseArray) { $caseById[$case.id] = $case }
if ($caseById['WP4_OFF'].h264.sha256 -ne $caseById['D4'].h264.sha256 -or
    $caseById['WP6_OFF'].h264.sha256 -ne $caseById['R6'].h264.sha256) {
    throw 'Weighted-P off controls are not byte-identical to their base cases.'
}

$manifest = [PSCustomObject]@{
    schema_version = 2
    matrix = 'H264_BOUNDARY_R2'
    generated_utc = [DateTime]::UtcNow.ToString('o')
    generator = 'tools/research/h264_hwcap/Generate-BoundaryMatrix.ps1'
    case_order = $caseOrder
    tools = [PSCustomObject]@{
        ffmpeg = (& $Ffmpeg -hide_banner -version 2>&1 | Select-Object -First 1).ToString()
        ffprobe = (& $Ffprobe -hide_banner -version 2>&1 | Select-Object -First 1).ToString()
        libx264 = $x264Build
        python = (& $Python --version 2>&1 | Select-Object -First 1).ToString()
    }
    fixed = [PSCustomObject]@{
        width = $width
        height = $height
        fps = $frameRate
        frame_count = $frameCount
        pixel_format = 'yuv420p'
        profile_idc = 100
        level_idc = 30
        keyint = 60
        scenecut = 0
        bframes = 0
        b_pyramid = 'none'
        weighted_bipred_idc = 0
        eos_nal_type = 11
    }
    source = [PSCustomObject]@{
        file = [IO.Path]::GetFileName($sourcePath)
        size = (Get-Item -LiteralPath $sourcePath).Length
        sha256 = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
        ffmpeg_filter = $sourceFilter
        command = Get-RecordedArguments $sourceArguments
    }
    cases = $caseArray
    validation = [PSCustomObject]@{
        field_constraints_passed = $true
        independent_parser_vs_trace_headers_passed = $true
        raw_h264_vs_mp4_golden_passed = $true
        explicit_eos_passed = $true
        dpb_rewrite_non_sps_identity_passed = $true
        weighted_controls_byte_identity_passed = $true
        header_admission_case_count = $caseArray.Count
        runtime_decode_case_count = $caseArray.Count
        warning_count = 0
    }
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'manifest.json'),
    ($manifest | ConvertTo-Json -Depth 14) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$ini = [Text.StringBuilder]::new()
[void]$ini.AppendLine('[General]')
[void]$ini.AppendLine('schema=2')
[void]$ini.AppendLine('matrix=H264_BOUNDARY_R2')
[void]$ini.AppendLine("count=$($caseArray.Count)")
[void]$ini.AppendLine("order=$($caseOrder -join ',')")
for ($caseIndex = 0; $caseIndex -lt $caseOrder.Count; ++$caseIndex) {
    [void]$ini.AppendLine("case$caseIndex=$($caseOrder[$caseIndex])")
}
[void]$ini.AppendLine("width=$width")
[void]$ini.AppendLine("height=$height")
[void]$ini.AppendLine("fps=$frameRate")
[void]$ini.AppendLine("frames=$frameCount")
foreach ($case in $caseArray) {
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
    [void]$ini.AppendLine("access_units=$($case.access_units)")
    [void]$ini.AppendLine('eos_nal=1')
    [void]$ini.AppendLine("golden_sha256=$($case.golden.aggregate_sha256)")
    [void]$ini.AppendLine('runtime_decode_eligible=true')
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'cases.ini'),
    $ini.ToString(),
    [Text.UTF8Encoding]::new($false))

Write-Host "Generated boundary matrix: $outputPath" -ForegroundColor Green
Write-Host "Cases: $($caseArray.Count)"
Write-Host "Order: $($caseOrder -join ' -> ')"
Write-Host "Manifest: $(Join-Path $outputPath 'manifest.json')"
