param(
    [ValidateSet('x64', 'x86')] [string[]] $Architectures = @('x64', 'x86'),
    [string] $PlatformToolset = '',
    [string] $OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install Visual Studio with Desktop development with C++.' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
if (!$OutputDirectory) { $OutputDirectory = Join-Path $repo 'dist\windows' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
foreach ($arch in $Architectures) {
    $platform = if ($arch -eq 'x86') { 'Win32' } else { 'x64' }
    $output = Join-Path $OutputDirectory $arch
    $intermediate = Join-Path $repo "dist\obj\windows-$arch"
    $arguments = @('-m', '-target:Build', '-p:Configuration=Release', "-p:Platform=$platform", ('-p:OutDir=' + $output.Replace('\', '/') + '/'), ('-p:IntDir=' + $intermediate.Replace('\', '/') + '/'))
    if ($PlatformToolset) { $arguments += "-p:PlatformToolset=$PlatformToolset" }
    & $msbuild (Join-Path $repo 'Sources\Omnikey\win32\Omnikey\Omnikey\Omnikey.vcxproj') @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($arch)." }
    & python (Join-Path $PSScriptRoot 'fetch-rime-windows.py') $arch $output
    if ($LASTEXITCODE -ne 0) { throw "Rime packaging failed ($arch)." }
    & (Join-Path $PSScriptRoot 'test-windows-rime.ps1') -Architecture $arch -Bundle (Join-Path $output 'Rime') -VisualStudio $vs
    $updaterIntermediate = Join-Path $repo "dist\obj\updater-$arch"
    $updaterArguments = @('-m', '-target:Build', '-p:Configuration=Release', "-p:Platform=$platform", ('-p:OutDir=' + $output.Replace('\', '/') + '/'), ('-p:IntDir=' + $updaterIntermediate.Replace('\', '/') + '/'))
    if ($PlatformToolset) { $updaterArguments += "-p:PlatformToolset=$PlatformToolset" }
    & $msbuild (Join-Path $repo 'Sources\Omnikey\win32\Omnikey\OmnikeyUpdate\OmnikeyUpdate.vcxproj') @updaterArguments
    if ($LASTEXITCODE -ne 0) { throw "Updater build failed ($arch)." }
    $helperDirectory = Join-Path $output "Rime\bin\$arch"
    New-Item -ItemType Directory -Force -Path $helperDirectory | Out-Null
    Copy-Item -LiteralPath (Join-Path $output 'OmnikeyUpdate.exe') -Destination (Join-Path $helperDirectory 'OmnikeyUpdate.exe') -Force
    Write-Host "Omnikey with Pinyin ready: $output"
}
