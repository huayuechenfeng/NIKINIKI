[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExtendedDllPath,

    [string]$Objdump = 'C:\QtSDK\Symbian\tools\gcce4\bin\arm-none-symbianelf-objdump.exe',

    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$dllPath = (Resolve-Path -LiteralPath $ExtendedDllPath).Path
$objdumpPath = (Resolve-Path -LiteralPath $Objdump).Path
$expectedSize = 54684
$expectedSha256 = '1DC529D328CF191E8BC5FC4A6B4C80DA5D47398F0A162714F28158C7DBB75994'
$actualSize = (Get-Item -LiteralPath $dllPath).Length
$actualSha256 = (Get-FileHash -LiteralPath $dllPath -Algorithm SHA256).Hash
if ($actualSize -ne $expectedSize -or $actualSha256 -ne $expectedSha256) {
    throw "Not the audited RM-779 SW113 extended image: size=$actualSize sha256=$actualSha256"
}

$imageBase = [Convert]::ToUInt32('80DBCF70', 16)
$bytes = [IO.File]::ReadAllBytes($dllPath)

function Read-U32 {
    param([int]$Offset)
    if ($Offset -lt 0 -or $Offset + 4 -gt $bytes.Length) {
        throw ('Read outside image at 0x{0:X}' -f $Offset)
    }
    [BitConverter]::ToUInt32($bytes, $Offset)
}

function Read-AsciiZ {
    param([int]$Offset)
    $end = $Offset
    while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) {
        ++$end
    }
    [Text.Encoding]::ASCII.GetString($bytes, $Offset, $end - $Offset)
}

function Disassemble-Thumb {
    param(
        [uint32]$Start,
        [uint32]$Stop
    )
    $text = & $objdumpPath -D -b binary -m arm -M force-thumb `
        --adjust-vma=0x80dbcf70 `
        ("--start-address=0x{0:x8}" -f $Start) `
        ("--stop-address=0x{0:x8}" -f $Stop) `
        $dllPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for 0x$($Start.ToString('X8'))..0x$($Stop.ToString('X8'))"
    }
    $text -join [Environment]::NewLine
}

$genericHeader = Disassemble-Thumb `
    -Start ([Convert]::ToUInt32('80DBDAC6', 16)) `
    -Stop ([Convert]::ToUInt32('80DBDB74', 16))
$spsParser = Disassemble-Thumb `
    -Start ([Convert]::ToUInt32('80DC6BE4', 16)) `
    -Stop ([Convert]::ToUInt32('80DC6E5C', 16))
$headerBuilder = Disassemble-Thumb `
    -Start ([Convert]::ToUInt32('80DC6FB0', 16)) `
    -Stop ([Convert]::ToUInt32('80DC72B2', 16))

$knownLowerStubAddresses = @(
    '80DBD050', # rcam ordinals 49/50
    '80DBD088', # rcam 36
    '80DBD090', # rcam 54
    '80DBD0A0', # IVE policy client 1
    '80DBD0E0', # rcam 52/53
    '80DBD108', # rcam 44
    '80DBD180', # IVE policy client 8
    '80DBD188', # IVE policy client 4
    '80DBD1D0', # IVE policy client 24
    '80DBD200', # rcam 4
    '80DBD218', # rcam 5
    '80DBD220', # rcam 55
    '80DBD228', # rcam 35
    '80DBD230', # rcam 11
    '80DBD238', # rcam 17
    '80DBD260', # rcam 25
    '80DBD298', # rcam 6
    '80DBD2B8', # rcam 47
    '80DBD2E0', # rcam 14
    '80DBD320', # rcam 40
    '80DBD340'  # rcam 3
)
$headerPathText = $genericHeader + [Environment]::NewLine +
    $spsParser + [Environment]::NewLine + $headerBuilder
$lowerCallsBeforeGate = @()
foreach ($address in $knownLowerStubAddresses) {
    if ($headerPathText -match ("(?i)0x" + $address)) {
        $lowerCallsBeforeGate += "0x$address"
    }
}

$checks = [ordered]@{
    avc_factory_uid = (Read-U32 0xBF14) -eq [Convert]::ToUInt32('10204C21', 16)
    avc_factory_function = (Read-U32 0xBF18) -eq [Convert]::ToUInt32('80DC2F5D', 16)
    avc_parser_rtti_name = (Read-AsciiZ 0xC181) -eq '32CIveVideoDecodeHwDeviceAVCParser'
    avc_parser_typeinfo = (Read-U32 0xCA88) -eq [Convert]::ToUInt32('80DC8F50', 16)
    avc_parser_method = (Read-U32 0xCA98) -eq [Convert]::ToUInt32('80DC6FB1', 16)
    generic_header_entry = $genericHeader -match '(?im)^80dbdac6:'
    generic_header_virtual_parser_call =
        $genericHeader -match '(?is)80dbdb38:.*ldr\s+r0, \[r4, #0\].*80dbdb44:.*ldr\s+r1, \[r0, #0\].*80dbdb48:.*ldr\s+r3, \[r1, #12\].*80dbdb50:.*blx\s+r7'
    max_num_ref_frames_store =
        $spsParser -match '(?is)80dc6d7c:.*bl\s+0x80dc63de.*80dc6d84:.*ldr\s+r1, \[sp, #4\].*80dc6d88:.*str\s+r0, \[r1, #0\]'
    ref_six_gate =
        $headerBuilder -match '(?is)80dc7220:.*ldr\s+r0, \[sp, #8\].*80dc7222:.*ldr\s+r0, \[r0, #0\].*80dc7224:.*cmp\s+r0, #6.*80dc7226:.*bhi.*0x80dc7234'
    kerr_not_supported_leave =
        $headerBuilder -match '(?is)80dc7234:.*movs\s+r0, #4.*80dc7236:.*mvns\s+r0, r0.*80dc7238:.*blx\s+0x80dbd0f0'
    no_known_rcam_or_policy_call_before_gate = $lowerCallsBeforeGate.Count -eq 0
}

$failedChecks = @($checks.GetEnumerator() | Where-Object { -not $_.Value } |
    ForEach-Object Key)
$report = [ordered]@{
    schema_version = 1
    scope = 'read-only static audit; no firmware code executed and no ROM/phone modification'
    target = [ordered]@{
        device = 'Nokia 603 / RM-779'
        firmware = 'SW113.010.1506 common core'
        file = $dllPath
        extended_size = $actualSize
        extended_sha256 = $actualSha256
        image_base = '0x80DBCF70'
        implementation_uid = '0x10204C21'
        dll_uid3 = '0x10204C1E'
    }
    symbols = [ordered]@{
        ecom_factory = '0x80DC2F5C'
        generic_get_header_information_l = '0x80DBDAC6'
        avc_parser_typeinfo = '0x80DC8F50'
        avc_parser_vtable_address_point = '0x80DC99FC'
        avc_parser_header_method = '0x80DC6FB0'
        max_num_ref_frames_store = '0x80DC6D88 (parser object + 0xC0)'
        admission_gate = '0x80DC7220..0x80DC7238'
    }
    decoded_gate = [ordered]@{
        primary_rule = 'max_num_ref_frames > 6 => User::Leave(KErrNotSupported)'
        additional_rule = 'coded_luma_pixels > 307200 and max_num_ref_frames >= 6 => KErrNotSupported'
        pixels_threshold = 307200
        result_for_640x360 = 'refs <= 6 accepted by this check; refs >= 7 rejected'
    }
    lower_boundary = [ordered]@{
        known_rcam_or_policy_calls_before_gate = $lowerCallsBeforeGate
        conclusion = 'The ref gate is inside the vendor AVCParser before any statically identified rcam or IVE policy-client call on this Header path.'
        physical_chip_handoff = 'NOT_REACHED_BY_ORIGINAL_REF7_AT_GETHEADER'
    }
    checks = $checks
    all_checks_passed = $failedChecks.Count -eq 0
    failed_checks = $failedChecks
}

$json = $report | ConvertTo-Json -Depth 10
if ($OutputPath) {
    $outputFullPath = [IO.Path]::GetFullPath($OutputPath)
    $parent = Split-Path -Parent $outputFullPath
    if ($parent -and -not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [IO.File]::WriteAllText(
        $outputFullPath,
        $json + [Environment]::NewLine,
        [Text.UTF8Encoding]::new($false))
}
$json

if ($failedChecks.Count -ne 0) {
    throw ('Static admission audit failed: ' + ($failedChecks -join ', '))
}
