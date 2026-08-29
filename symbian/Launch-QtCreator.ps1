[CmdletBinding()]
param(
    [string]$QtSdkRoot = 'C:\QtSDK',
    [string]$Project = (Join-Path $PSScriptRoot 'probes\qt-minimal\qt-minimal.pro')
)

$creator = Join-Path $QtSdkRoot 'QtCreator\bin\qtcreator.exe'
if (-not (Test-Path -LiteralPath $creator -PathType Leaf)) {
    throw "Qt Creator was not found at $creator"
}

$resolvedProject = (Resolve-Path -LiteralPath $Project -ErrorAction Stop).Path
$previousCompatibility = $env:__COMPAT_LAYER

try {
    # Qt Creator 2.4.1 otherwise starts without a usable window on this Windows 11 host.
    $env:__COMPAT_LAYER = 'WIN7RTM'
    Start-Process `
        -FilePath $creator `
        -ArgumentList ('"' + $resolvedProject + '"') `
        -WorkingDirectory (Split-Path -Parent $resolvedProject) `
        -WindowStyle Normal
}
finally {
    if ($null -eq $previousCompatibility) {
        Remove-Item Env:__COMPAT_LAYER -ErrorAction SilentlyContinue
    }
    else {
        $env:__COMPAT_LAYER = $previousCompatibility
    }
}
