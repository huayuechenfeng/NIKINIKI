[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$probeRoot = Join-Path $repositoryRoot 'symbian\probes\devvideo-capability'
$probeProject = Join-Path $probeRoot 'devvideo-capability.pro'
$probeSource = Join-Path $probeRoot 'main.cpp'
$productFiles = @(
    (Join-Path $repositoryRoot 'symbian\app\wiliwili_symbian.pro'),
    (Join-Path $repositoryRoot 'symbian\Build-App.ps1')
)

foreach ($path in @($probeProject, $probeSource) + $productFiles) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required isolation input is missing: $path"
    }
}

$projectText = Get-Content -LiteralPath $probeProject -Raw
$sourceText = Get-Content -LiteralPath $probeSource -Raw
$projectCode = [regex]::Replace($projectText, '(?m)#.*$', '')
if ($projectCode -notmatch '(?m)^TARGET\s*=\s*nikiniki_devvideo_capability_probe\s*$') {
    throw 'Research probe target identity is missing.'
}
if ($projectCode -notmatch '(?m)^\s*TARGET\.UID3\s*=\s*0xE000B11D\s*$') {
    throw 'Research probe UID is missing or changed.'
}
if ($projectCode -match '0xE000B100|wiliwili_symbian|MOBILITY|QtMobility|symbian[/\\]source') {
    throw 'Research probe project crosses the product identity or source boundary.'
}
if ($projectCode -notmatch '(?m)^SOURCES\s*\+=\s*main\.cpp\s*$') {
    throw 'Research probe must compile only its local main.cpp.'
}
if ($sourceText -match 'CVideoPlayerUtility|QMediaPlayer|ffmpeg|bilibili|CustomInterface\s*\(') {
    throw 'Research probe contains a forbidden product/network/guessing dependency.'
}

foreach ($path in $productFiles) {
    $text = Get-Content -LiteralPath $path -Raw
    if ($text -match 'devvideo-capability|nikiniki_devvideo_capability_probe|E000B11D') {
        throw "Product build references the research probe: $path"
    }
}

Write-Host 'PASS: Direct DevVideo capability probe is isolated from the product build.'
Write-Host "Probe target: nikiniki_devvideo_capability_probe / UID 0xE000B11D"
