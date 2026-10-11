# Building Omnikey

This guide covers building and testing the jetaudio fork of Omnikey on macOS and Windows.
Vietnamese instructions for macOS are in [macOS_Build.md](macOS_Build.md).

## macOS

### Requirements

- macOS with **full Xcode**. The fork is tested with Xcode 27. The deployment target is macOS 12.0.
- Network access for the first build, which downloads librime and the Rime data.
- The Command Line Tools are enough to run the regression tests. The app itself needs full Xcode.

### Build a universal DMG

```bash
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer bash scripts/build-macos-dmg.sh
```

The script does the following:

1. Builds the `Omnikey` scheme in Release for `arm64` and `x86_64`.
2. Checks that the storyboard, `librime.1.dylib` and the compiled Pinyin dictionary are embedded, and that both architectures are present.
3. Signs the app ad hoc and verifies the signature and `Info.plist`.
4. Writes three files to `dist/`:
   - `Omnikey-<version>-<commit>-universal.dmg`
   - its `.sha256` checksum
   - a `-BUILD-INFO.txt` file that records the source commit and Xcode version

The result is **not notarized**.

### Build from Xcode

Open `Sources/Omnikey/macOS/Omnikey.xcodeproj`, select the **Omnikey** scheme and choose **Product → Run** or **Product → Archive**. The **Embed Rime** build phase runs `scripts/fetch-rime.sh` automatically.

### Rime (Chinese input) dependencies

`scripts/fetch-rime.sh` prepares everything the Chinese mode needs in `Sources/Omnikey/macOS/ThirdParty/Rime`. Git ignores that folder.

| Component | Pinned version | License |
| --- | --- | --- |
| librime | 1.17.0, universal macOS build | BSD 3-Clause |
| rime-prelude | commit `082425e` | LGPL-3.0 |
| rime-essay | commit `054920d` | LGPL-3.0 |
| rime-pinyin-simp | commit `0c6861e` | Apache-2.0 |

Each download is checked against a SHA-256 hash in the script, and the `pinyin_simp` dictionary is precompiled. A stamp file skips the work when nothing has changed. To force a refresh, delete the folder.

Omnikey's own Rime configuration is in `Sources/Omnikey/macOS/Rime/default.custom.yaml`.

## Windows

### Requirements

- Visual Studio 2022 with the **Desktop development with C++** workload, which includes MSBuild and the Windows SDK.

### Build

Open `Sources/Omnikey/win32/Omnikey/Omnikey.sln` in Visual Studio and build **Release** for **Win32** or **x64**.

To build from a Developer PowerShell, as CI does:

```powershell
foreach ($arch in 'x86', 'x64') {
  $platform = if ($arch -eq 'x86') { 'Win32' } else { 'x64' }
  foreach ($project in 'Omnikey', 'OmnikeyUpdate') {
    msbuild "Sources\Omnikey\win32\Omnikey\$project\$project.vcxproj" -m -target:Rebuild `
      -p:Configuration=Release "-p:Platform=$platform" `
      "-p:OutDir=$PWD\ArtifactOutput\$arch\" "-p:IntDir=$PWD\build\obj\$project\$arch\"
  }
}
```

The outputs are `Omnikey32.exe` or `Omnikey64.exe`, plus `OmnikeyUpdate.exe`, in `ArtifactOutput\<arch>\`.

### Windows Pinyin input

The Windows app supports simplified Chinese Pinyin through the same Rime 1.17.0
engine and dictionaries as macOS. To build a complete runnable package:

```powershell
./scripts/build-windows.ps1
# With Visual Studio 2026 Build Tools:
./scripts/build-windows.ps1 -PlatformToolset v145
```

This builds the x64 and x86 app/updater, verifies the downloaded Rime DLL and
dictionary archives against pinned SHA-256 hashes, precompiles the Pinyin
dictionary, and runs the engine and input integration tests for each architecture.
Outputs are in `dist/windows/x64` and `dist/windows/x86`. Python 3 and Windows
`tar` (or `7z`) are also required. The first packaging run requires network access.
When building directly in Visual Studio, run
`python scripts/fetch-rime-windows.py x64 <exe-output-directory>` (or `x86`)
after the build. Keep the `Rime` directory beside the executable when distributing
or copying it; copying the EXE alone omits Chinese input.

Select **Trung (Pinyin)** in the control panel or **Gõ tiếng Trung (Pinyin)** in
the tray menu. Type `nihao`, then Space for `你好`. Numbers 1–7 or a click select
candidates; PgUp/PgDn change pages, Backspace edits, and Esc cancels. The panel
does not take focus. Application shortcuts and a focus change cancel composition.
The existing switch hotkey cycles Vietnamese → English → Chinese → Vietnamese.
Clicking the tray icon returns from Chinese to Vietnamese. The candidate popup
uses a compact horizontal layout, rounded translucent surface and selection cells,
a soft shadow, and clickable page chevrons, matching the macOS panel. It follows
the Windows app light/dark theme and scales with the target application's DPI;
on narrow displays, candidates wrap rather than extending off-screen.
Chinese mode stays selected across applications; per-app Vietnamese/
English preferences do not override it. Learned phrases are stored in
`%LOCALAPPDATA%\Omnikey\Rime`.

Rime starts in the background. Missing or incompatible Rime data is reported when
Chinese mode is selected; Vietnamese and English input remain available.
Windows input injection follows the target application's privilege level. If
an elevated application rejects committed text, Omnikey offers the committed
text on the clipboard for manual paste.

Standalone Pinyin tests (with a packaged `Rime` directory):

```powershell
./scripts/test-windows-rime.ps1 -Architecture x64
```

The input integration tests mock desktop text injection, focus and panel visibility
so they do not type into any real application or use the user's Rime learning data.

## Tests

| Script | What it covers | Requires |
| --- | --- | --- |
| `scripts/test-engine.sh` | Typing engine (tone placement in both orthographies), code-table conversion, smart-switch preferences. Runs under ASan/UBSan. | Command Line Tools |
| `scripts/test-macos-events.sh` | Event tap callback, key sending, Spotlight, Fn shortcuts, Chinese mode and tap lifecycle. All system calls are mocked. | Command Line Tools |
| `scripts/test-rime.sh` | Pinyin input, candidates, learning and forgetting phrases through librime. | Command Line Tools, network on first run |
| `scripts/test-macos-ui.sh <app> <png>` | Compiled storyboard outlets, layout geometry and the Fn action. Renders the preferences window to a PNG. Set `OMNIKEY_UI_APPEARANCE=dark` to render the dark appearance. | A built app and a GUI session |

On Windows, CI also compiles `tests/engine_regression.cpp` with MSVC (`cl /std:c++14 /utf-8`) and runs it with Windows key codes.

## Publishing a release

The in-app updaters depend on the following layout. Keep it the same for every release.

1. Raise the version:
   - macOS: `MARKETING_VERSION` and `CURRENT_PROJECT_VERSION` in the Xcode project
   - Windows: `FILEVERSION`/`PRODUCTVERSION` in `Omnikey.rc`
2. Update `version.json` on `master`:
   - `latestVersion.versionCode` is the macOS build number (`CFBundleVersion`).
   - `latestWinVersion.versionCode` is `major | minor << 8 | patch << 16`. For example, 2.0.6 is `393218`.
3. Tag the release `v<version>`, for example `v2.0.6`.
4. Attach these assets:
   - `Omnikey-<version>-macOS-universal.dmg`
   - `Omnikey-<version>-Windows.zip`, with `Omnikey32.exe`, `Omnikey64.exe` and `OmnikeyUpdate.exe` at the root, and the complete `Rime` directory. Both DLL/helper architectures live under `Rime/bin/x64` and `Rime/bin/x86`.
   - Optional architecture-specific `Omnikey-<version>-Windows-x64.zip` and `-x86.zip`
   - `SHA256SUMS.txt`

Create and validate all three Windows ZIPs and checksums with:

```powershell
python scripts/package-windows-release.py 2.1.0 dist/windows dist/release
```

The script checks EXE versions, PE architectures and ZIP integrity. Run the Rime
tests for both architectures against `dist/release/staging/2.1.0/combined/Rime`
as well; this checks that their shared dictionary works with either DLL.

`python tests/windows_update_package_tests.py dist/release/Omnikey-2.1.0-Windows.zip`
tests the apply script embedded in the packaged updater. It verifies x64/x86
updates and repeat updates, and checks that corrupt ZIPs, missing dictionaries
and locked application files fail without deleting the previous application.

The Windows updater downloads `releases/download/v<version>/Omnikey-<version>-Windows.zip`,
waits for extraction, and copies both the matching EXE and its Rime payload. The
app launches a temporary copy of its bundled helper so the update can replace
the helper too. An initial update through the legacy 2.0.6 helper leaves Rime in
`_OmnikeyUpdate/Rime`; the new app can use those files and their new helper.
The macOS app opens the latest release page. For a Windows-only release, leave
the macOS version metadata unchanged and link or reattach its current DMG.

## Continuous integration

| Workflow | Trigger | Output |
| --- | --- | --- |
| [Build macOS DMG](.github/workflows/macos-dmg.yml) | Pushes to `master` that touch the engine, macOS sources, tests or scripts. Can also be started manually. | Runs the engine, Rime and event tests, then builds and verifies the universal DMG. Artifact: **Omnikey-macos-universal** |
| [MSBuild](.github/workflows/msbuild.yml) | Pushes and pull requests to `master`. Can also be started manually. | Runs the engine tests with MSVC, then builds the x86 and x64 app and updater and checks each PE machine type. Artifact: **Omnikey**, with a build-provenance attestation |
