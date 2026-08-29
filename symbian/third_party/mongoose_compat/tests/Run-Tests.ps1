[CmdletBinding()]
param()

# Build and run the standalone mongoose-compat tests with a host C compiler.
# Uses the first available of: cl (via vswhere), gcc, clang.
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $here 'mg_json_test.c'
$impl = Join-Path $here '..\mg_json.c'
$out = Join-Path $here 'mg_json_test.exe'

$cl = $null
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
if (Test-Path -LiteralPath $vswhere) {
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vs) {
        $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
        if (Test-Path -LiteralPath $vcvars) {
            $cl = 'cl'
        }
    }
}

if ($cl) {
    $batch = 'call "' + $vcvars + '" >nul 2>&1 && cl /nologo /W3 /utf-8 "'
    $batch += $src + '" "' + $impl + '" /Fe:"' + $out + '"'
    & $env:ComSpec /d /s /c $batch
    if ($LASTEXITCODE -ne 0) { throw "cl build failed ($LASTEXITCODE)" }
} elseif (Get-Command gcc -ErrorAction SilentlyContinue) {
    & gcc -std=c99 -Wall -Wextra -o $out $src $impl
    if ($LASTEXITCODE -ne 0) { throw "gcc build failed ($LASTEXITCODE)" }
} elseif (Get-Command clang -ErrorAction SilentlyContinue) {
    & clang -std=c99 -Wall -Wextra -o $out $src $impl
    if ($LASTEXITCODE -ne 0) { throw "clang build failed ($LASTEXITCODE)" }
} else {
    throw 'No host C compiler found (cl/gcc/clang).'
}

& $out
exit $LASTEXITCODE
