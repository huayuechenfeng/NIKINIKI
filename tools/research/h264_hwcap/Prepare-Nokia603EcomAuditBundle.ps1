[CmdletBinding()]
param(
    [string]$SisPath,
    [string]$OutputDirectory,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
if (-not $SisPath) {
    $SisPath = Join-Path $repositoryRoot (
        'symbian\out\devvideo-ecom-audit-release\nikiniki_devvideo_ecom_audit.sis')
}
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot (
        '.tmp\h264-hwcap\nokia603-ecom-audit-r3-bundle')
}
$sisFullPath = [IO.Path]::GetFullPath($SisPath)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $sisFullPath -PathType Leaf)) {
    throw "ECom audit SIS is missing: $sisFullPath"
}
if (Test-Path -LiteralPath $outputPath) {
    if (-not (Test-Path -LiteralPath $outputPath -PathType Container)) {
        throw "Output exists but is not a directory: $outputPath"
    }
    if ((Get-ChildItem -LiteralPath $outputPath -Force | Select-Object -First 1) -and -not $Force) {
        throw "Output directory is not empty. Use -Force to overwrite named files: $outputPath"
    }
}
$installDirectory = Join-Path $outputPath 'INSTALL'
New-Item -ItemType Directory -Path $installDirectory -Force | Out-Null
$sisDestination = Join-Path $installDirectory 'nikiniki_devvideo_ecom_audit.sis'
Copy-Item -LiteralPath $sisFullPath -Destination $sisDestination -Force

$manifest = [PSCustomObject]@{
    schema_version = 3
    purpose = 'NIKINIKI_NOKIA603_ECOM_SPI_ARCHIVE_AUDIT'
    interface_uid = '0x101FB4BE'
    mdf_processing_unit_interface_uid = '0x10273789'
    implementation_uid = '0x10204C21'
    resource_parser = 'ECOM_RSC_RESOURCE_1_V1_V2_V3'
    spi_archive_directory = 'Z:/private/10009D8F/'
    spi_archive_base_name = 'ecom'
    spi_archive_type_uid = '0x10205C2C'
    probe = [PSCustomObject]@{
        file = 'INSTALL/nikiniki_devvideo_ecom_audit.sis'
        uid = '0xE000B11E'
        size = (Get-Item -LiteralPath $sisDestination).Length
        sha256 = (Get-FileHash -LiteralPath $sisDestination -Algorithm SHA256).Hash
    }
    safety = [PSCustomObject]@{
        creates_decoder = $false
        calls_custom_interface = $false
        writes_rom_or_registry = $false
        requests_allfiles_or_tcb = $false
    }
}
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'bundle-manifest.json'),
    ($manifest | ConvertTo-Json -Depth 6) + [Environment]::NewLine,
    [Text.UTF8Encoding]::new($false))

$instructions = @'
NIKINIKI Nokia 603 read-only DevVideo ECom audit

1. Install INSTALL\nikiniki_devvideo_ecom_audit.sis. It is version 0.3.0 and
   upgrades/replaces the earlier audit app with the same UID.
2. Launch NIKINIKI DevVideo ECom Audit.
3. Tap Export read-only ECom audit once and wait for Complete.
4. Copy or zip the displayed result directory. On this Nokia 603 it should be:
   F:\Data\NIKINIKI\hwcap\ecom-audit\<timestamp>\

The probe repeats the published DevVideo/MDF inventory and loose-RSC parse, then
uses the same public RResourceArchive call as the ECom server to open the
read-only Z:\private\10009D8F\ ecom SPI archive set. If platform security denies
that read, the denial code is a valid result: do not change phone permissions.
It does not create a decoder, call CustomInterface, modify ROM/registry files,
or request AllFiles/TCB capabilities.
'@
[IO.File]::WriteAllText(
    (Join-Path $outputPath 'RUN_ON_NOKIA603_ECOM_R3.txt'),
    $instructions,
    [Text.UTF8Encoding]::new($false))

Write-Host "Nokia 603 read-only ECom audit bundle prepared: $outputPath" -ForegroundColor Green
Write-Host "Probe SIS SHA-256: $($manifest.probe.sha256)"
