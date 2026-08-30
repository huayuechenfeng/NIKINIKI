[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$markdownFiles = Get-ChildItem -LiteralPath $repositoryRoot -Recurse -File -Filter '*.md' |
    Where-Object {
        $_.FullName -notmatch '[\\/]\.git[\\/]' -and
        $_.FullName -notmatch '[\\/]symbian[\\/]out[\\/]'
    }

$errors = New-Object System.Collections.Generic.List[string]
$linkPattern = [regex]'!?(?:\[[^\]]*\])\((?<target>[^)]+)\)'

foreach ($file in $markdownFiles) {
    $content = [IO.File]::ReadAllText($file.FullName)
    $linkContent = [regex]::Replace($content, '(?s)```.*?```', '')
    $linkContent = [regex]::Replace($linkContent, '`[^`\r\n]*`', '')

    foreach ($match in $linkPattern.Matches($linkContent)) {
        $target = $match.Groups['target'].Value.Trim()

        if ($target.StartsWith('<') -and $target.EndsWith('>')) {
            $target = $target.Substring(1, $target.Length - 2)
        }

        # Drop an optional Markdown title after the path.
        if ($target -match '^(?<path>\S+)\s+["''].*["'']$') {
            $target = $Matches['path']
        }

        if ([string]::IsNullOrWhiteSpace($target) -or
            $target.StartsWith('#') -or
            $target -match '^[a-zA-Z][a-zA-Z0-9+.-]*:' -or
            $target.StartsWith('//')) {
            continue
        }

        $localTarget = ($target -split '[?#]', 2)[0]
        if ([string]::IsNullOrWhiteSpace($localTarget)) {
            continue
        }

        $localTarget = [Uri]::UnescapeDataString($localTarget)
        $candidate = Join-Path -Path $file.DirectoryName -ChildPath $localTarget

        if (-not (Test-Path -LiteralPath $candidate)) {
            $relativeFile = $file.FullName.Substring($repositoryRoot.Length + 1)
            $errors.Add("$relativeFile -> $target")
        }
    }
}

$activeFiles = @(
    'AGENTS.md',
    'README.md',
    'docs/README_ZH.md',
    'docs/STATUS_ZH.md',
    'docs/ROADMAP_ZH.md'
)

$retiredRootPaths = @(
    'docs/DEVELOPMENT_STAGE_REPORT_2026-08-25_ZH.md',
    'docs/DEVELOPMENT_DESIGN_ZH.md',
    'docs/NEXT_WORK_PLAN_ZH.md',
    'docs/PLAYER_1.0_DECODING_POLICY_ZH.md',
    'docs/PLAYER_1.0_SOFTWARE_DECODER_PLAN_ZH.md',
    'docs/DEVICE_TEST_MATRIX.md',
    'docs/RELEASE_1.0.0_ZH.md'
)

foreach ($relativeFile in $activeFiles) {
    $fullPath = Join-Path -Path $repositoryRoot -ChildPath $relativeFile
    if (-not (Test-Path -LiteralPath $fullPath)) {
        $errors.Add("missing active document: $relativeFile")
        continue
    }

    $content = [IO.File]::ReadAllText($fullPath)
    foreach ($retiredPath in $retiredRootPaths) {
        if ($content.Contains($retiredPath)) {
            $errors.Add("$relativeFile references retired path $retiredPath")
        }
    }
}

if ($errors.Count -gt 0) {
    Write-Host 'Documentation checks failed:' -ForegroundColor Red
    foreach ($errorMessage in $errors) {
        Write-Host "  $errorMessage" -ForegroundColor Red
    }
    exit 1
}

Write-Host ("Documentation checks passed: {0} Markdown files" -f $markdownFiles.Count) -ForegroundColor Green
