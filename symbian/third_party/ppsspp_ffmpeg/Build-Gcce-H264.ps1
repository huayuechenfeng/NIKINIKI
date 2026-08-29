[CmdletBinding()]
param(
    [string]$SourceDirectory,
    [string]$SdkRoot = 'C:\QtSDK\Symbian\SDKs\Symbian3Qt474',
    [string]$BuildDirectory,
    [string]$GitBash,
    [string]$MsysBash,
    [string]$MakePath,
    [string]$GcceBin,
    [string]$ExpectedSourceCommit = 'b87f7c6d522d1edba77cfc4fac96ce48a236f806',
    [switch]$EnableArmAssembly,
    [switch]$Reconfigure
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$workspaceRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
if (-not $SourceDirectory) {
    $SourceDirectory = Join-Path $workspaceRoot '.tmp\ppsspp-ffmpeg-research'
}
if (-not $BuildDirectory) {
    $buildLeaf = if ($EnableArmAssembly) { 'gcce-armv6asm' } else { 'gcce' }
    $BuildDirectory = Join-Path $PSScriptRoot "build\$buildLeaf"
}
$SourceDirectory = [IO.Path]::GetFullPath($SourceDirectory)
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$SdkRoot = [IO.Path]::GetFullPath($SdkRoot)
$symbianRoot = Split-Path -Parent (Split-Path -Parent $SdkRoot)

if (-not $GitBash) {
    $GitBash = 'C:\Program Files\Git\bin\bash.exe'
}
if (-not $MsysBash) {
    $MsysBash = 'C:\msys64\usr\bin\bash.exe'
}
if (-not $MakePath) {
    $MakePath = Join-Path $symbianRoot 'tools\sbs\win32\mingw\bin\make.exe'
}
if (-not $GcceBin) {
    $GcceBin = Join-Path $symbianRoot 'tools\gcce4\bin'
}
$GitBash = [IO.Path]::GetFullPath($GitBash)
$MsysBash = [IO.Path]::GetFullPath($MsysBash)
$MakePath = [IO.Path]::GetFullPath($MakePath)
$GcceBin = [IO.Path]::GetFullPath($GcceBin)

function Convert-ToBashPath([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path).Replace('\', '/')
    if ($full -notmatch '^([A-Za-z]):/(.*)$') {
        throw "Cannot convert path for Git Bash: $Path"
    }
    return '/' + $Matches[1].ToLowerInvariant() + '/' + $Matches[2]
}

function Quote-Bash([string]$Value) {
    $singleQuote = [string][char]39
    $replacement = $singleQuote + '"' + $singleQuote + '"' + $singleQuote
    return $singleQuote + $Value.Replace($singleQuote, $replacement) + $singleQuote
}

$configure = Join-Path $SourceDirectory 'configure'
foreach ($required in @(
        $configure,
        (Join-Path $SourceDirectory 'LICENSE.md'),
        (Join-Path $SourceDirectory 'COPYING.LGPLv2.1'),
        (Join-Path $SourceDirectory 'doc\Makefile'),
        (Join-Path $SourceDirectory 'tests\Makefile'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Incomplete PPSSPP-FFmpeg checkout: $required is missing. Sparse checkouts must include doc and tests."
    }
}

$git = Get-Command git -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $git) {
    throw 'Git is required to verify the pinned PPSSPP-FFmpeg source revision.'
}
$actualSourceCommit = (& $git.Source -C $SourceDirectory rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualSourceCommit -ne $ExpectedSourceCommit) {
    throw "Unexpected PPSSPP-FFmpeg revision. Expected $ExpectedSourceCommit, got $actualSourceCommit."
}
$sourceStatus = & $git.Source -C $SourceDirectory status --porcelain
if ($LASTEXITCODE -ne 0 -or $sourceStatus) {
    throw 'PPSSPP-FFmpeg checkout must be clean before building release libraries.'
}

$gitRoot = Split-Path -Parent (Split-Path -Parent $GitBash)
$gitUsrBin = Join-Path $gitRoot 'usr\bin'
foreach ($tool in @($GitBash, $MakePath, (Join-Path $GcceBin 'arm-none-symbianelf-gcc.exe'))) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required build tool is missing: $tool"
    }
}
if ($EnableArmAssembly -and -not (Test-Path -LiteralPath $MsysBash -PathType Leaf)) {
    throw "ARM assembly builds require the case-sensitive MSYS2 make driver: $MsysBash"
}

$ownedBuildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'build'))
if ($Reconfigure -and (Test-Path -LiteralPath $BuildDirectory)) {
    if (-not $BuildDirectory.StartsWith(
            $ownedBuildRoot + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase) -and
        $BuildDirectory -ne $ownedBuildRoot) {
        throw "Refusing to remove a build directory outside $ownedBuildRoot"
    }
    Remove-Item -LiteralPath $BuildDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path $BuildDirectory -Force | Out-Null

$sourceBash = Convert-ToBashPath $SourceDirectory
$buildBash = Convert-ToBashPath $BuildDirectory
$gcceBinBash = Convert-ToBashPath $GcceBin
$makeBinBash = Convert-ToBashPath (Split-Path -Parent $MakePath)
$sdkUnix = $SdkRoot.Replace('\', '/')
$include = "$sdkUnix/epoc32/include"
$cflags = @(
    '-D__EPOC32__', '-D__MARM_ARMV5__', '-D__EABI__',
    '-D__SUPPORT_CPP_EXCEPTIONS__', '-nostdinc',
    "-I$include", "-I$include/platform", "-I$include/stdapis",
    "-I$include/stdapis/sys", "-I$include/stdapis/stlportv5",
    "-I$include/stdapis/stlportv5/stl", '-Os', '-Wno-psabi',
    '-fno-short-enums', '-fno-strict-aliasing', '-finline-limit=300',
    '-DHAVE_UNISTD_H', '-mfloat-abi=softfp', '-mfpu=vfp', '-marm'
) -join ' '
$ldflags = @(
    "-L$sdkUnix/epoc32/release/armv5/lib",
    "-L$sdkUnix/epoc32/release/armv5/urel",
    '-nostdlib'
) -join ' '

$arguments = @(
    '--target-os=symbian', '--enable-cross-compile',
    '--arch=arm', '--cpu=arm1176jzf-s',
    "--sysinclude=$include",
    '--cc=arm-none-symbianelf-gcc',
    '--cxx=arm-none-symbianelf-g++',
    '--ar=arm-none-symbianelf-ar',
    '--ranlib=arm-none-symbianelf-ranlib',
    '--nm=arm-none-symbianelf-nm',
    '--strip=arm-none-symbianelf-strip',
    '--host-cc=false',
    '--disable-everything', '--enable-decoder=h264',
    '--disable-shared', '--enable-static',
    '--disable-pthreads', '--disable-w32threads', '--disable-os2threads',
    '--disable-avformat', '--disable-avdevice', '--disable-avfilter',
    '--disable-swresample', '--disable-swscale', '--disable-postproc',
    '--disable-programs', '--disable-doc', '--disable-network',
    '--disable-iconv', '--disable-zlib', '--disable-bzlib',
    '--disable-lzma', '--disable-sdl',
    "--extra-cflags=$cflags", "--extra-ldflags=$ldflags"
)
if ($EnableArmAssembly) {
    # ARM1176JZF-S has ARMv6/VFPv2 but no NEON. Keep FFmpeg's ARM/ARMv6
    # assembly while preventing any NEON object or runtime dispatch from
    # entering the Nokia 603 package. In this FFmpeg revision the H.264 qpel,
    # chroma, prediction, weighted prediction, deblock and IDCT fast paths are
    # NEON-only; ARMv6 mainly accelerates start-code scanning, and CABAC's ARM
    # inline path requires ARMv6T2. Do not describe this as an ARM11-optimized
    # H.264 core merely because ARM assembly objects are present.
    # The Symbian GCCE link layout starts .data at 0x400000. A whole-library
    # -O2 build makes the linked H.264 .text/.rodata cross that fixed boundary.
    # Keep size optimization while retaining the ARMv6/VFP assembly paths.
    $arguments += @('--disable-neon', '--enable-small')
} else {
    $arguments += @('--disable-asm', '--disable-inline-asm', '--enable-small')
}

if ($Reconfigure -or -not (Test-Path -LiteralPath (Join-Path $BuildDirectory 'config.mak'))) {
    $bashPath = "$gcceBinBash`:$makeBinBash`:/usr/bin:/mingw64/bin:`$PATH"
    $quotedArguments = ($arguments | ForEach-Object { Quote-Bash $_ }) -join ' '
    $command = "set -e; export PATH=" + (Quote-Bash $bashPath) +
        "; cd " + (Quote-Bash $buildBash) +
        "; " + (Quote-Bash "$sourceBash/configure") + " $quotedArguments"
    & $GitBash -lc $command
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg configure failed with exit code $LASTEXITCODE."
    }
}

# Git Bash writes /c/... source paths, but the SDK's native MinGW make only
# accepts C:/... paths. Normalize generated files after every configure.
$sourceUnix = $SourceDirectory.Replace('\', '/')
foreach ($name in @('Makefile', 'makefile', 'config.mak')) {
    $path = Join-Path $BuildDirectory $name
    if (Test-Path -LiteralPath $path) {
        $text = [IO.File]::ReadAllText($path)
        $text = $text.Replace($sourceBash, $sourceUnix)
        [IO.File]::WriteAllText($path, $text, [Text.Encoding]::ASCII)
    }
}

if ($EnableArmAssembly) {
    # The native MinGW make bundled with SBS case-folds the .S prerequisite to
    # .s and cannot find FFmpeg's ARM assembly files. MSYS2 make preserves the
    # suffix. Keep its temporary files inside the workspace so GCCE and MSYS2
    # do not need write access to C:\msys64\tmp.
    $temporaryDirectory = Join-Path $workspaceRoot '.tmp\gcce-temp'
    New-Item -ItemType Directory -Path $temporaryDirectory -Force | Out-Null
    $temporaryBash = Convert-ToBashPath $temporaryDirectory
    $buildCommand = @(
        'set -e',
        "export PATH=$gcceBinBash`:/usr/bin:`$PATH",
        "export TMPDIR=$(Quote-Bash $temporaryBash)",
        'export TEMP=$TMPDIR',
        'export TMP=$TMPDIR',
        "cd $(Quote-Bash $buildBash)",
        'make -j2 libavcodec/libavcodec.a libavutil/libavutil.a'
    ) -join '; '
    & $MsysBash -lc $buildCommand
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg ARM assembly make failed with exit code $LASTEXITCODE."
    }
} else {
    $previousPath = $env:PATH
    try {
        $env:PATH = "$GcceBin;$gitUsrBin;$(Split-Path -Parent $MakePath);$previousPath"
        Push-Location $BuildDirectory
        try {
            & $MakePath -j2 libavcodec/libavcodec.a libavutil/libavutil.a
            if ($LASTEXITCODE -ne 0) {
                throw "FFmpeg make failed with exit code $LASTEXITCODE."
            }
        } finally {
            Pop-Location
        }
    } finally {
        $env:PATH = $previousPath
    }
}

$includeRoot = Join-Path $PSScriptRoot 'include'
$codecInclude = Join-Path $includeRoot 'libavcodec'
$utilInclude = Join-Path $includeRoot 'libavutil'
$libraryRoot = Join-Path $PSScriptRoot 'lib\gcce'
New-Item -ItemType Directory -Path $codecInclude,$utilInclude,$libraryRoot -Force | Out-Null
$codecHeaders = @(
    (Join-Path $SourceDirectory 'libavcodec\avcodec.h'),
    (Join-Path $SourceDirectory 'libavcodec\version.h')
)
Copy-Item -LiteralPath $codecHeaders -Destination $codecInclude -Force
Get-ChildItem -LiteralPath (Join-Path $SourceDirectory 'libavutil') -File -Filter '*.h' |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $utilInclude -Force }
Copy-Item -LiteralPath (Join-Path $BuildDirectory 'libavutil\avconfig.h') -Destination $utilInclude -Force

foreach ($library in @('libavcodec', 'libavutil')) {
    $archive = Join-Path $BuildDirectory "$library\$library.a"
    Copy-Item -LiteralPath $archive -Destination (Join-Path $libraryRoot "$library.a") -Force
    Copy-Item -LiteralPath $archive -Destination (Join-Path $libraryRoot "$library.lib") -Force
}
$licenseFiles = @(
    (Join-Path $SourceDirectory 'LICENSE.md'),
    (Join-Path $SourceDirectory 'COPYING.LGPLv2.1')
)
Copy-Item -LiteralPath $licenseFiles -Destination $PSScriptRoot -Force

$nm = Join-Path $GcceBin 'arm-none-symbianelf-nm.exe'
$codecSymbols = & $nm -g --defined-only (Join-Path $libraryRoot 'libavcodec.a')
foreach ($symbol in @('avcodec_find_decoder', 'avcodec_open2', 'avcodec_decode_video2', 'ff_h264_decoder')) {
    if (-not ($codecSymbols | Select-String -SimpleMatch $symbol)) {
        throw "Built archive is missing required symbol: $symbol"
    }
}
if ($EnableArmAssembly) {
    $configuration = Get-Content -LiteralPath (Join-Path $BuildDirectory 'config.mak') -Raw
    foreach ($requiredSetting in @(
            'HAVE_ARMV6=yes', 'HAVE_VFP=yes', 'HAVE_INLINE_ASM=yes',
            '!HAVE_NEON=yes')) {
        if (-not $configuration.Contains($requiredSetting)) {
            throw "Unexpected FFmpeg ARM configuration; missing: $requiredSetting"
        }
    }
    foreach ($symbol in @('ff_h264dsp_init_arm', 'ff_startcode_find_candidate_armv6')) {
        if (-not ($codecSymbols | Select-String -SimpleMatch $symbol)) {
            throw "Built archive is missing required ARMv6 symbol: $symbol"
        }
    }
}

Write-Host "PPSSPP-FFmpeg H.264 GCCE artifacts: $PSScriptRoot"
Write-Host "Verified PPSSPP-FFmpeg commit: $actualSourceCommit"
Write-Host "ARM assembly enabled: $EnableArmAssembly"
