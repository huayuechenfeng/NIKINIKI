[CmdletBinding()]
param(
    [string]$RepositoryRoot,
    [switch]$RunHostTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepositoryRoot) {
    $RepositoryRoot = Join-Path $PSScriptRoot '..'
}
$root = (Resolve-Path -LiteralPath $RepositoryRoot).Path

function Assert-Condition([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

function Normalize-RelativePath([string]$Path) {
    return $Path.Replace('\', '/')
}

$git = Get-Command git -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $git) {
    throw 'Git is required to validate the public repository boundary.'
}
$trackedRelativePaths = @(& $git.Source -C $root ls-files)
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to enumerate tracked repository files.'
}
$trackedRelativePaths = @($trackedRelativePaths | ForEach-Object { Normalize-RelativePath $_ })
$trackedPathSet = [Collections.Generic.HashSet[string]]::new(
    [StringComparer]::OrdinalIgnoreCase)
foreach ($relative in $trackedRelativePaths) {
    [void]$trackedPathSet.Add($relative)
}

function Test-TrackedPath([string]$RelativePath) {
    $normalized = Normalize-RelativePath $RelativePath
    foreach ($tracked in $trackedRelativePaths) {
        if ($tracked -eq $normalized -or $tracked.StartsWith("$normalized/", [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

$trackedFiles = foreach ($relative in $trackedRelativePaths) {
    $fullPath = Join-Path $root ($relative.Replace('/', [IO.Path]::DirectorySeparatorChar))
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "Tracked file is missing from checkout: $relative"
    }
    [PSCustomObject]@{
        RelativePath = $relative
        FullName = $fullPath
        Extension = [IO.Path]::GetExtension($relative).ToLowerInvariant()
    }
}

$requiredFiles = @(
    'README.md',
    'AGENTS.md',
    'LICENSE',
    'NOTICE.md',
    'docs/UPSTREAM_BASELINE.md',
    'docs/CODE_BOUNDARY_ANALYSIS_ZH.md',
    'symbian/reuse-manifest.yml',
    'symbian/Build-App.ps1',
    'symbian/Build-Probe.ps1',
    'symbian/third_party/ppsspp_ffmpeg/Build-Gcce-H264.ps1',
    'symbian/app/wiliwili_symbian.pro',
    'symbian/app/resources.qrc'
)
foreach ($relative in $requiredFiles) {
    Assert-Condition $trackedPathSet.Contains($relative) "Required repository file is missing: $relative"
}

$forbiddenRootDirectories = @(
    'wiliwili', 'library', 'resources', 'cmake', 'scripts', 'winrt'
)
foreach ($relative in $forbiddenRootDirectories) {
    Assert-Condition (-not (Test-TrackedPath $relative)) "Research/upstream directory entered the product repository: $relative"
}

$forbiddenPaths = @(
    'symbian/archive',
    'symbian/out',
    'symbian/LOCAL_ARTIFACTS.md'
)
foreach ($relative in $forbiddenPaths) {
    Assert-Condition (-not (Test-TrackedPath $relative)) "Private or generated path entered the product repository: $relative"
}

$forbiddenExtensions = @(
    '.sis', '.sisx', '.exe', '.dll', '.lib', '.a', '.o', '.obj',
    '.pfx', '.p12', '.key', '.cer', '.csr', '.dmp'
)
$allowedBinaryPaths = @(
    'prerequisites/Qt-4.7.403-for-Anna.sis',
    'prerequisites/QtMobility-1.2.1-for-Anna.sis'
)
$forbiddenFiles = $trackedFiles | Where-Object {
    $forbiddenExtensions -contains $_.Extension -and
    $allowedBinaryPaths -notcontains $_.RelativePath
}
Assert-Condition `
    (-not $forbiddenFiles) `
    ("Forbidden generated/signing files found:" + [Environment]::NewLine +
        (($forbiddenFiles | ForEach-Object RelativePath) -join [Environment]::NewLine))

$textExtensions = @(
    '.c', '.cpp', '.h', '.hpp', '.pro', '.qrc', '.ps1', '.md', '.yml',
    '.yaml', '.json', '.mjs', '.py', '.txt')
$textFiles = $trackedFiles | Where-Object { $textExtensions -contains $_.Extension }
$sourceBoundaryFiles = $textFiles | Where-Object {
    $_.RelativePath -like 'symbian/app/*' -or
    $_.RelativePath -like 'symbian/source/*' -or
    $_.RelativePath -like 'symbian/include/*' -or
    $_.RelativePath -like 'symbian/generated/*'
}
$upstreamPathPattern = '(?i)(\.\.[\\/]){2,}(wiliwili|library|resources)([\\/]|$)'
$upstreamPathMatches = foreach ($file in $sourceBoundaryFiles) {
    Select-String -LiteralPath $file.FullName -Pattern $upstreamPathPattern -List
}
Assert-Condition `
    (-not $upstreamPathMatches) `
    ("Product build files reference the external research tree:" + [Environment]::NewLine +
        ($upstreamPathMatches -join [Environment]::NewLine))

$sensitiveTextFiles = $textFiles | Where-Object { $_.FullName -ne $PSCommandPath }
$forbiddenTextPatterns = @(
    '(?i)[A-Z]:[\\/]Users[\\/]',
    '(?<![0-9.])(?:10(?:\.[0-9]{1,3}){3}|192\.168(?:\.[0-9]{1,3}){2}|172\.(?:1[6-9]|2[0-9]|3[01])(?:\.[0-9]{1,3}){2})(?![0-9.])',
    '-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----'
)
foreach ($pattern in $forbiddenTextPatterns) {
    $matches = foreach ($file in $sensitiveTextFiles) {
        Select-String -LiteralPath $file.FullName -Pattern $pattern
    }
    Assert-Condition `
        (-not $matches) `
        ("Forbidden path or sensitive marker found for pattern '$pattern':" +
            [Environment]::NewLine + ($matches -join [Environment]::NewLine))
}

$markdownFailures = @()
$textFiles | Where-Object { $_.Extension -eq '.md' } | ForEach-Object {
    $document = $_.FullName
    $text = Get-Content -LiteralPath $document -Raw
    $markdownText = [regex]::Replace($text, '(?s)```.*?```', '')
    $markdownText = [regex]::Replace($markdownText, '`[^`\r\n]*`', '')
    foreach ($match in [regex]::Matches(
            $markdownText, '!?(?:\[[^\]\r\n]+\])\(([^)]+)\)')) {
        $target = $match.Groups[1].Value.Trim().Trim('<', '>')
        if ($target -match '^(?:https?://|mailto:|#)') {
            continue
        }
        $target = $target.Split('#')[0]
        if (-not $target) {
            continue
        }
        $decodedTarget = [uri]::UnescapeDataString($target)
        $combinedTarget = Join-Path `
            -Path (Split-Path -Parent $document) `
            -ChildPath $decodedTarget
        $resolved = [IO.Path]::GetFullPath($combinedTarget)
        if (-not (Test-Path -LiteralPath $resolved)) {
            $relativeDocument = $document.Substring($root.Length + 1)
            $markdownFailures += "$relativeDocument -> $target"
        }
    }
}
Assert-Condition `
    (-not $markdownFailures) `
    ("Broken relative Markdown links:" + [Environment]::NewLine +
        ($markdownFailures -join [Environment]::NewLine))

$powerShellFiles = $textFiles | Where-Object { $_.Extension -eq '.ps1' }
foreach ($file in $powerShellFiles) {
    $tokens = $null
    $errors = $null
    [Management.Automation.Language.Parser]::ParseFile(
        $file.FullName, [ref]$tokens, [ref]$errors) | Out-Null
    if ($errors.Count -ne 0) {
        $parserMessages = $errors | ForEach-Object Message
        throw ("PowerShell parser errors in $($file.RelativePath):" +
            [Environment]::NewLine + ($parserMessages -join [Environment]::NewLine))
    }
}

if ($RunHostTests) {
    & (Join-Path $root 'symbian\third_party\mongoose_compat\tests\Run-Tests.ps1')
    if ($LASTEXITCODE -ne 0) {
        throw "Host JSON tests failed with exit code $LASTEXITCODE"
    }
}

Write-Host 'PASS: canonical NIKINIKI repository validation'
Write-Host "Root: $root"
Write-Host "Checked tracked source files: $($textFiles.Count)"
Write-Host "Checked tracked PowerShell files: $($powerShellFiles.Count)"
