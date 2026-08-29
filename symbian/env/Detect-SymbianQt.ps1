[CmdletBinding()]
param(
    [string[]]$SearchRoot,
    [switch]$Json,
    [switch]$RequireReady
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Add-CandidateRoot {
    param([System.Collections.Generic.List[string]]$List, [string]$Path)
    if ($Path -and (Test-Path -LiteralPath $Path)) {
        $resolved = (Resolve-Path -LiteralPath $Path).Path
        if (-not $List.Contains($resolved)) {
            $List.Add($resolved)
        }
    }
}

function Find-Tool {
    param(
        [string[]]$Names,
        [System.Collections.Generic.List[string]]$Roots
    )

    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) {
            return $command.Source
        }
    }

    foreach ($root in $Roots) {
        foreach ($name in $Names) {
            $match = Get-ChildItem -LiteralPath $root -Recurse -File -Filter $name -ErrorAction SilentlyContinue |
                Sort-Object @{ Expression = {
                    # Build the public application against the oldest supported
                    # SDK so one binary can run on Symbian^3, Anna and Belle.
                    # Belle remains an installed fallback/probe target.
                    if ($_.FullName -match '(?i)symbian3qt474') { 0 }
                    elseif ($_.FullName -match '(?i)symbiansr1qt474|belle') { 1 }
                    elseif ($_.FullName -match '(?i)symbian|gcce|epoc32') { 2 }
                    else { 3 }
                } }, FullName |
                Select-Object -First 1
            if ($match) {
                return $match.FullName
            }
        }
    }

    return $null
}

$candidateRoots = [System.Collections.Generic.List[string]]::new()
if ($SearchRoot) {
    foreach ($root in $SearchRoot) {
        Add-CandidateRoot -List $candidateRoots -Path $root
    }
} else {
    foreach ($root in @(
        $env:QTDIR,
        $env:EPOCROOT,
        'C:\QtSDK',
        'C:\NokiaQtSDK',
        'C:\Nokia',
        'C:\Symbian',
        'C:\S60',
        'C:\Program Files (x86)\Nokia',
        'C:\Program Files\Nokia'
    )) {
        Add-CandidateRoot -List $candidateRoots -Path $root
    }
}

$qmake = Find-Tool -Names @('qmake.exe', 'qmake') -Roots $candidateRoots
$make = Find-Tool -Names @('make.exe', 'make', 'mingw32-make.exe') -Roots $candidateRoots
$compiler = Find-Tool -Names @('arm-none-symbianelf-g++.exe', 'gcce.exe', 'mwccsym2.exe') -Roots $candidateRoots
$makeSis = Find-Tool -Names @('makesis.exe', 'makesis') -Roots $candidateRoots
$signSis = Find-Tool -Names @('signsis.exe', 'signsis') -Roots $candidateRoots
$perl = Find-Tool -Names @('perl.exe', 'perl') -Roots $candidateRoots
$environmentBatch = Find-Tool -Names @('qtenv2.bat', 'qtenv.bat', 'env.bat') -Roots $candidateRoots

$qtVersion = $null
$qmakeSpec = $null
if ($qmake) {
    try {
        $qtVersion = (& $qmake -query QT_VERSION 2>$null | Select-Object -First 1)
        $qmakeSpec = (& $qmake -query QMAKE_SPEC 2>$null | Select-Object -First 1)
        if (-not $qmakeSpec -or $qmakeSpec -eq '**Unknown**') {
            $qmakeRoot = Split-Path -Parent (Split-Path -Parent $qmake)
            $defaultSpec = Join-Path $qmakeRoot 'mkspecs\default\qmake.conf'
            if (Test-Path -LiteralPath $defaultSpec) {
                $specText = Get-Content -LiteralPath $defaultSpec -Raw
                if ($specText -match '(?m)^QMAKESPEC_ORIGINAL=.*/mkspecs/([^\r\n]+)') {
                    $qmakeSpec = $Matches[1].Trim()
                }
            }
        }
    } catch {
        $qtVersion = 'query failed'
    }
}

$required = @($qmake, $make, $compiler, $makeSis, $signSis, $perl)
$ready = @($required | Where-Object { -not $_ }).Count -eq 0

$result = [pscustomobject]@{
    Ready = $ready
    SearchRoots = @($candidateRoots)
    QtVersion = $qtVersion
    QmakeSpec = $qmakeSpec
    Qmake = $qmake
    Make = $make
    Compiler = $compiler
    MakeSis = $makeSis
    SignSis = $signSis
    Perl = $perl
    EnvironmentBatch = $environmentBatch
    EpocRoot = $env:EPOCROOT
}

if ($Json) {
    $result | ConvertTo-Json -Depth 4
} else {
    $result | Format-List
}

if ($RequireReady -and -not $ready) {
    throw 'The Symbian Qt toolchain is incomplete. See the missing fields above.'
}
