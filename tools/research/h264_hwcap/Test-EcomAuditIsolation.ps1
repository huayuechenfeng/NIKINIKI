[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$probeRoot = Join-Path $repositoryRoot 'symbian\probes\devvideo-ecom-audit'
$probeProject = Join-Path $probeRoot 'devvideo-ecom-audit.pro'
$probeSource = Join-Path $probeRoot 'main.cpp'
$productFiles = @(
    (Join-Path $repositoryRoot 'symbian\app\wiliwili_symbian.pro'),
    (Join-Path $repositoryRoot 'symbian\Build-App.ps1')
)

foreach ($path in @($probeProject, $probeSource) + $productFiles) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required ECom audit isolation input is missing: $path"
    }
}

$projectText = Get-Content -LiteralPath $probeProject -Raw
$sourceText = Get-Content -LiteralPath $probeSource -Raw
$projectCode = [regex]::Replace($projectText, '(?m)#.*$', '')
if ($projectCode -notmatch '(?m)^TARGET\s*=\s*nikiniki_devvideo_ecom_audit\s*$' -or
    $projectCode -notmatch '(?m)^\s*TARGET\.UID3\s*=\s*0xE000B11E\s*$') {
    throw 'Read-only ECom audit target identity is missing or changed.'
}
if ($projectCode -match '0xE000B100|wiliwili_symbian|MOBILITY|QtMobility|symbian[/\\]source') {
    throw 'ECom audit project crosses the product identity or source boundary.'
}
if ($projectCode -match '(?m)^\s*TARGET\.CAPABILITY\s*[+]?=') {
    throw 'ECom audit must retain the qmake default CAPABILITY None.'
}
if ($projectCode -notmatch '(?m)^SOURCES\s*\+=\s*main\.cpp\s*$' -or
    $projectCode -notmatch '(?m)^\s*LIBS\s*\+=.*-lecom' -or
    $projectCode -notmatch '(?m)^\s*LIBS\s*\+=.*-lefsrv' -or
    $projectCode -notmatch '(?m)^\s*LIBS\s*\+=.*-lbafl' -or
    $projectCode -notmatch '(?m)^\s*LIBS\s*\+=.*-lhash') {
    throw 'ECom audit must compile only its local main.cpp and published ECom/resource/hash libraries.'
}
if ($sourceText -match 'CreateImplementationL|CMMFDevVideoPlay|CustomInterface\s*\(|WriteCodedData|QMediaPlayer|ffmpeg|bilibili') {
    throw 'ECom audit contains a forbidden decoder creation, mutation, product or network dependency.'
}
if ($sourceText -notmatch 'REComSession::ListImplementationsL' -or
    $sourceText -notmatch 'Z:/resource/plugins' -or
    $sourceText -notmatch 'RResourceArchive' -or
    $sourceText -notmatch 'Z:\\\\private\\\\10009D8F\\\\' -or
    $sourceText -match 'QIODevice::WriteOnly[^\r\n]*(Z:/|Z:\\)') {
    throw 'ECom audit is missing the published read-only path or attempts a ROM write.'
}

foreach ($path in $productFiles) {
    $text = Get-Content -LiteralPath $path -Raw
    if ($text -match 'devvideo-ecom-audit|nikiniki_devvideo_ecom_audit|E000B11E') {
        throw "Product build references the ECom research probe: $path"
    }
}

Write-Host 'PASS: DevVideo ECom audit is read-only and isolated from the product build.'
Write-Host 'Probe target: nikiniki_devvideo_ecom_audit / UID 0xE000B11E'
