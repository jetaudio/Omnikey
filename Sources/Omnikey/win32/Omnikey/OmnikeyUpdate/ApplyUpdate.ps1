param([ValidateSet('Omnikey64.exe', 'Omnikey32.exe', 'OpenKey64.exe', 'OpenKey32.exe')] [string] $MainExe)
$ErrorActionPreference = 'Stop'
try {
    # The package must match its line in the release's SHA256SUMS.txt.
    $expected = Get-Content -LiteralPath './_OmnikeyUpdate.sha256' |
        ForEach-Object { if ($_ -match '^([0-9a-fA-F]{64})\s+\*?Omnikey-[0-9.]+-Windows\.zip\s*$') { $Matches[1] } } |
        Select-Object -First 1
    # .NET directly: Get-FileHash is not available in every Windows PowerShell session.
    $stream = [System.IO.File]::OpenRead((Resolve-Path -LiteralPath './_OmnikeyUpdate.zip').ProviderPath)
    try { $actual = [System.BitConverter]::ToString([System.Security.Cryptography.SHA256]::Create().ComputeHash($stream)).Replace('-', '') }
    finally { $stream.Dispose() }
    if (!$expected -or $actual -ne $expected) { throw 'Checksum mismatch' }
    Expand-Archive -LiteralPath './_OmnikeyUpdate.zip' -DestinationPath './_OmnikeyUpdate' -Force
    if (!(Test-Path -LiteralPath './_OmnikeyUpdate/Rime/build/pinyin_simp.table.bin')) { throw 'Missing Pinyin dictionary' }
    # The package always names the app Omnikey; an install from the OpenKey days keeps its EXE name.
    $sourceExe = Join-Path './_OmnikeyUpdate' ($MainExe -replace '^OpenKey', 'Omnikey')
    if (!(Test-Path -LiteralPath $sourceExe)) { throw 'Missing application' }
    $copied = $false
    for ($attempt = 0; $attempt -lt 20; $attempt++) {
        try {
            Copy-Item -LiteralPath './_OmnikeyUpdate/Rime' -Destination './' -Recurse -Force
            Copy-Item -LiteralPath $sourceExe -Destination (Join-Path './' $MainExe) -Force
            $copied = $true
            break
        } catch { Start-Sleep -Milliseconds 250 }
    }
    if (!$copied) { throw 'Could not replace application files' }
    exit 0
} catch { exit 1 }
