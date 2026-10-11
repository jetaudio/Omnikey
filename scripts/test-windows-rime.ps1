param(
    [ValidateSet('x64', 'x86')] [string] $Architecture = 'x64',
    [string] $Bundle = '',
    [string] $VisualStudio = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (!$VisualStudio) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $VisualStudio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if (!$VisualStudio) { throw 'Install Visual Studio C++ Build Tools.' }
if (!$Bundle) { $Bundle = Join-Path $repo "dist\windows\$Architecture\Rime" }
$Bundle = [IO.Path]::GetFullPath($Bundle)
$testOutput = Join-Path $repo "dist\tests\$Architecture"
New-Item -ItemType Directory -Force -Path $testOutput | Out-Null
$batch = Join-Path $testOutput 'rime-tests.cmd'
@"
@echo off
call "$VisualStudio\VC\Auxiliary\Build\vcvarsall.bat" $Architecture
if errorlevel 1 exit /b 1
cd /d "$testOutput"
cl /nologo /EHsc /std:c++14 /utf-8 "$repo\Sources\Omnikey\win32\Omnikey\Omnikey\WindowsRime.cpp" "$repo\tests\windows_rime_tests.cpp" /Fe:windows-rime-tests.exe /link shell32.lib user32.lib
if errorlevel 1 exit /b 1
windows-rime-tests.exe "$Bundle" "$testOutput\user"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE "$repo\Sources\Omnikey\win32\Omnikey\Omnikey\WindowsRime.cpp" "$repo\Sources\Omnikey\win32\Omnikey\Omnikey\CandidatePanel.cpp" "$repo\tests\windows_chinese_input_tests.cpp" /Fe:chinese-input-tests.exe /link shell32.lib user32.lib gdi32.lib ole32.lib uuid.lib advapi32.lib
if errorlevel 1 exit /b 1
chinese-input-tests.exe "$(Split-Path -Parent $Bundle)" "$testOutput\integration-user"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++14 /utf-8 /DUNICODE /D_UNICODE "$repo\tests\windows_update_restart_tests.cpp" /Fe:restart-regression.exe
if errorlevel 1 exit /b 1
restart-regression.exe
exit /b %errorlevel%
"@ | Set-Content -LiteralPath $batch -Encoding ASCII
& cmd /c $batch
if ($LASTEXITCODE -ne 0) { throw "Rime regression tests failed ($Architecture)." }
