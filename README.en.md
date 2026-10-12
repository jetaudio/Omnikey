# Omnikey

[![Build macOS DMG](https://github.com/jetaudio/Omnikey/actions/workflows/macos-dmg.yml/badge.svg)](https://github.com/jetaudio/Omnikey/actions/workflows/macos-dmg.yml)
[![MSBuild](https://github.com/jetaudio/Omnikey/actions/workflows/msbuild.yml/badge.svg)](https://github.com/jetaudio/Omnikey/actions/workflows/msbuild.yml)
[![Build Linux DEB](https://github.com/jetaudio/Omnikey/actions/workflows/linux-deb.yml/badge.svg)](https://github.com/jetaudio/Omnikey/actions/workflows/linux-deb.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Release](https://img.shields.io/github/v/release/jetaudio/Omnikey)](https://github.com/jetaudio/Omnikey/releases/latest)
![Platforms](https://img.shields.io/badge/platforms-macOS%2012%2B%20%7C%20Windows%20%7C%20Linux-lightgrey)

**One open-source Vietnamese input method, with one shared engine on macOS, Windows and Linux (Ubuntu).**

## Why this fork exists

I got tired of the Vietnamese input methods on Ubuntu and macOS: underlined words in some, missing characters in others, broken typing in the terminal, and a different input method to learn on every machine. Meanwhile I'm hooked on UniKey on Windows. It just works, with no underline and nothing to think about.

So I forked OpenKey and improved it, to get **one input method that types the same way on every platform**, feeling as close to UniKey as possible.

## Origin

Omnikey was previously called **OpenKey (jetaudio fork)**. The old `jetaudio/OpenKey` repository redirects here.
Moving from OpenKey to Omnikey:
- **Windows:** existing OpenKey installs still update to Omnikey automatically. Settings are copied over, and the old start-with-Windows entry is removed.
- **macOS:** settings are copied over automatically. Because the app was renamed, **grant Accessibility access again** for Omnikey and delete the old `OpenKey.app` so the two do not run together.
- **Linux:** the `ibus-omnikey` package replaces `ibus-openkey`. Settings and macros are copied over; open the Omnikey app once to switch the old input source to Omnikey.

This repository is a maintained fork of [OpenKey](https://github.com/tuyenvm/OpenKey) by Mai Vũ Tuyên.
It builds on upstream `master` at [`89c2fd3`](https://github.com/tuyenvm/OpenKey/commit/89c2fd3bf258562f2349f89b49d81e2f140c3fc3).
It adds a new Linux (IBus) port, a Chinese input mode, a redesigned macOS interface, fixes taken from open upstream pull requests, regression tests and CI builds.

[Tiếng Việt](README.md) · [Changelog](CHANGELOG.md) · [Building](BUILDING.md) · [Upstream PR review](PR_REVIEW.md)

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/images/preferences-dark.png">
  <img alt="Omnikey preferences on macOS" src="docs/images/preferences-light.png">
</picture>

---

## Table of contents

- [Why this fork exists](#why-this-fork-exists)
- [What this fork adds](#what-this-fork-adds)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Building from source](#building-from-source)
- [Testing](#testing)
- [Project structure](#project-structure)
- [Known limitations](#known-limitations)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

## What this fork adds

Changes compared with upstream `tuyenvm/OpenKey` at `89c2fd3`. See [CHANGELOG.md](CHANGELOG.md) for the full list.

### New features

| Area | Change |
| --- | --- |
| **Chinese input (macOS and Windows)** | A third input mode, **中**, for Simplified Chinese Pinyin. It uses [librime](https://github.com/rime/librime) with the official `pinyin_simp` schema and a compact candidate panel. Phrases are learned as you type; **Shift+Delete** forgets a learned phrase (macOS also supports **Control+K**). The mode shortcut cycles **Tiếng Việt → English → 中**. |
| **Linux port (Ubuntu, GNOME, Wayland and X11)** | An IBus engine built on the same engine as macOS and Windows. **UniKey-style typing without underlines:** a `uinput` virtual keyboard sends real BackSpace keys, so typing works in terminals too. Apps that support it are edited in place. An underlined-word mode is still available. It handles autocomplete suggestions in browser address bars and notices when you click elsewhere. A GTK 4/libadwaita settings app follows the latest Ubuntu design and applies changes immediately. The Vietnamese/English switch is Ctrl+Shift (default), Ctrl+Space or Alt+Z, and the top bar shows **V/E**. |
| **macOS interface** | The preferences, macro, convert-tool and About windows are redesigned in the macOS 26 (Tahoe) settings style. They use toolbar tabs, rounded groups, switches, inline pop-ups and a segmented modifier-key control. |
| **Globe/Fn shortcut (macOS)** | Fn (🌐) can be used alone or with other modifiers to switch input modes. |
| **Simple Telex 2 (Windows)** | Simple Telex 1 and 2 are available in the Windows settings and tray menu. Previously, the engine supported Simple Telex 2 but Windows did not offer it. |

### Reliability fixes

These fixes were adapted from eight open upstream pull requests: #287, #289, #297, #317, #324, #329, #332 and #333. Each one was reviewed, partly rewritten and covered by tests. [PR_REVIEW.md](PR_REVIEW.md) records every decision.

- **macOS event tap:** re-enabled after macOS disables it on timeout, with a 0.5 s watchdog. Each replacement allocates fresh backspace events. Long strings are sent in complete UTF-16 chunks.
- **Spotlight:** text is replaced in place only when the Spotlight field really has focus. Hidden or fading windows no longer count, and focus queries time out after 20 ms.
- **Login items:** `SMAppService` is used on macOS 13 and later.
- **Developer apps:** apps such as Terminal start in English, but your saved choices and code table are kept.
- **Windows lock and unlock:** keyboard hooks are restored on `WTS_SESSION_UNLOCK`. If installing a new hook fails, the old hook is kept.
- **Windows clipboard paste:** a buffer overrun and a stray trailing character in the paste path are fixed.
- **Convert tool:** letter case is kept when marks are removed, and single-byte VNI and CP1258 characters still decode correctly.
- **Mode switching:** the Vietnamese engine starts a fresh word when you enter or leave Chinese mode.

### Engineering

- **Regression tests:** run under AddressSanitizer and UndefinedBehaviorSanitizer.
  - The engine suite has 3,400+ assertions, including tone placement in old and modern orthography.
  - The macOS suites cover event processing, the event tap lifecycle and Chinese input. System calls are mocked, so the tests never send keys or install hooks on the host.
- **CI:** GitHub Actions builds a universal (`arm64` + `x86_64`) DMG and Windows x86/x64 executables. The Windows workflow verifies each PE machine type and publishes a build-provenance attestation.
- **Performance:** less work per keystroke. The frontmost app and the input source are looked up once per key, and the engine avoids repeated map lookups and copies.

## Features

Inherited from OpenKey and available on both platforms unless noted:

- **Input methods:** Telex, VNI, Simple Telex 1, Simple Telex 2.
- **Code tables:** Unicode (precomposed), TCVN3 (ABC), VNI Windows, Unicode Compound, Vietnamese Locale CP1258.
- **Typing:** modern (`oà`, `uý`) or old (`òa`, `úy`) tone placement, spell checking, restoring keys when a word is invalid, and Quick Telex (`cc`→`ch`, `gg`→`gi`, `kk`→`kh`, `nn`→`ng`, `qq`→`qu`, `pp`→`ph`, `tt`→`th`).
- **Shorthand for consonants:** initials (`f`→`ph`, `j`→`gi`, `w`→`qu`) and endings (`g`→`ng`, `h`→`nh`, `k`→`ch`).
- **Text expansion:** macros with no length limit, which can be imported from and exported to text files.
- **Smart switching:** remembers the input mode and code table for each application.
- **Temporary overrides:** Control turns off spell checking and Command/Alt turns off Omnikey while held.
- **Automatic capitals:** capitalizes the first letter of each sentence.
- **Convert tool:** converts text between code tables and changes letter case, with a configurable hotkey.
- **Browser fixes:** works around autocomplete in browsers and Microsoft Excel, with an optional Chromium-specific fix.
- **Other languages:** optionally turns off Vietnamese while a non-English system input source is active.

## Requirements

| Platform | Requirement |
| --- | --- |
| macOS | macOS 12 Monterey or later, on Apple Silicon or Intel |
| Windows | Windows Vista or later, x86 or x64 |
| Linux | IBus and libadwaita 1.5 or later. Tested on Ubuntu 26.04 (GNOME 50, Wayland) |

## Installation

### macOS

1. Download `Omnikey-<version>-macOS-universal.dmg` from the [latest release](https://github.com/jetaudio/Omnikey/releases/latest). Development builds are available as the **Omnikey-macos-universal** artifact of [Build macOS DMG](https://github.com/jetaudio/Omnikey/actions/workflows/macos-dmg.yml) runs, or you can [build it yourself](#building-from-source).
2. Open the DMG and drag **Omnikey.app** to **Applications**.
3. The app has an ad hoc signature and is not notarized. On first launch, right-click it and choose **Open**, or allow it in **System Settings → Privacy & Security**.
4. Grant Accessibility access in **System Settings → Privacy & Security → Accessibility**. Keep it enabled while Omnikey runs.

> [!IMPORTANT]
> Turn off other Vietnamese input methods while using Omnikey. Two input methods running together interfere with each other.

### Windows

1. Download `Omnikey-<version>-Windows.zip` from the [latest release](https://github.com/jetaudio/Omnikey/releases/latest), or the **Omnikey** artifact of an [MSBuild](https://github.com/jetaudio/Omnikey/actions/workflows/msbuild.yml) run.
2. Extract the entire ZIP, keep the `Rime` folder beside the EXE, and run `Omnikey64.exe` on 64-bit Windows or `Omnikey32.exe` on 32-bit Windows. Separate x64/x86 ZIPs are also available. In the 2.0.5 package the EXEs are in `x64/` and `x86/`.
3. Accept the administrator prompt. Omnikey runs elevated so that it can type into games and elevated apps.

### Linux (Ubuntu)

1. Build the `.deb` package (see [Building from source](#building-from-source)).
2. Install it and restart IBus:
   ```bash
   sudo apt install ./dist/ibus-omnikey_*_amd64.deb
   ibus restart
   ```
3. Open the **Omnikey** app and click **Thêm Omnikey** (Add Omnikey), or go to **Settings → Keyboard → Input Sources → + → Vietnamese → Omnikey**.
4. For UniKey-style typing without underlines, choose **Gõ phím → Sửa trực tiếp** (Typing → Edit in place) in the app.

## Usage

- **Switch input mode:** the configured shortcut cycles **Vietnamese → English → Chinese → Vietnamese** on macOS and Windows (Windows defaults to **Alt+Z**). Linux has only **V ↔ E** and defaults to **Ctrl+Shift**. You can also choose a mode from the menu bar or tray icon.
- **Use Globe (🌐) as the shortcut on macOS:** in **System Settings → Keyboard**, set *Press 🌐 key to* **Do Nothing**.
- **Chinese mode:**
  - Type Pinyin.
  - **Space** commits the first candidate, and **1–7** or a click selects another one.
  - **−/=** or **Page Up/Page Down** change the page.
  - **Esc** cancels.
  - **Caps Lock** types Latin letters.
- **Macros and the convert tool:** open them from the menu bar or tray menu.

## Building from source

See [BUILDING.md](BUILDING.md) for full instructions. In short:

```bash
# macOS (full Xcode required); writes the DMG to dist/
bash scripts/build-macos-dmg.sh
```

The first macOS build needs network access. `scripts/fetch-rime.sh` downloads pinned releases of librime and the Rime data, checks their SHA-256 hashes and precompiles the dictionary.

On Windows, build `Sources/Omnikey/win32/Omnikey/Omnikey.sln` with Visual Studio or MSBuild.

On Ubuntu, the script runs the engine tests, builds the engine and settings app, and writes a `.deb` to `dist/`:

```bash
sudo apt install build-essential cmake pkg-config libibus-1.0-dev libadwaita-1-dev
bash scripts/build-linux.sh
```

## Testing

```bash
bash scripts/test-engine.sh        # typing engine, code tables, smart switch (ASan/UBSan)
bash scripts/test-macos-events.sh  # macOS event processing and event tap lifecycle
bash scripts/test-rime.sh          # Chinese Pinyin input through librime
bash scripts/test-macos-ui.sh /path/to/Omnikey.app /tmp/preferences.png  # UI layout QA (needs a GUI session)
```

The first three suites need only the Command Line Tools. CI runs all of them on every push to `master`, and runs the engine tests again on Windows with MSVC and Windows key codes.

## Project structure

```text
Sources/Omnikey/
├── engine/          Cross-platform C++ typing engine, macros, code-table conversion
├── macOS/
│   ├── ModernKey/   macOS app: event tap, UI, Rime bridge (OKRime), candidate panel
│   └── Rime/        Rime configuration bundled with the app
├── win32/           Windows app and updater (Visual Studio solution)
└── linux/           Linux port: IBus engine, GTK 4/libadwaita settings app, packaging
scripts/             Build, Rime fetch and test scripts
tests/               Regression and UI QA tests
docs/images/         Screenshots
```

## Known limitations

- **Unsigned builds:** builds are signed ad hoc and not notarized.
- **Update checks:** from 2.0.6, the app checks this repository for new versions. Version 2.0.5 still reads the upstream `version.json`, so it does not announce new fork releases; download 2.0.6 manually. On macOS, builds with the new updater download, verify and install the DMG after confirmation, then relaunch. Older builds need one manual installation. Releases must include a universal DMG and its `.dmg.sha256` file; the application directory must be writable. The Windows updater replaces only `Omnikey64.exe`.
- **No long-term interactive testing:** these situations have not been tested by hand over a long period:
  - typing in Apple Mail, WebKit and Spotlight
  - sleep and wake
  - Login Items
  - Windows lock and unlock

  The automated tests mock these system interactions.
- **Chinese input:** available on macOS and Windows only, and only for Simplified Chinese Pinyin.
- **Linux:**
  - The port is new. It has been tested on Ubuntu 26.04 with GNOME apps, Firefox, LibreOffice and Terminal.
  - Edit-in-place mode needs access to `/dev/uinput`. The package installs a udev rule that gives the logged-in user this access, so any program running as that user can also inject key presses.
  - Apps that do not report the cursor position to IBus, such as terminals, cannot tell the input method when you click elsewhere in the middle of a word.
  - Input methods use QWERTY key positions.

## Contributing

Issues and pull requests are welcome.

1. Fork the repository and create a branch from `master`.
2. Keep changes focused, and match the style of the surrounding code.
3. Add or update tests in `tests/` when you change behavior, and run the test scripts above.
4. Open a pull request that describes the change and how you verified it.

Fixes to the shared engine also benefit upstream. Consider proposing them to [tuyenvm/OpenKey](https://github.com/tuyenvm/OpenKey) too.

## License

Omnikey is free software under the [GNU General Public License v3.0](LICENSE). As the license requires, this fork stays open source and credits the original project, OpenKey.

The macOS app bundles these third-party components. Their license texts ship in `Omnikey.app/Contents/Resources/Rime/licenses`.

| Component | License |
| --- | --- |
| [librime](https://github.com/rime/librime) | BSD 3-Clause |
| [rime-prelude](https://github.com/rime/rime-prelude) | LGPL-3.0 |
| [rime-essay](https://github.com/rime/rime-essay) | LGPL-3.0 |
| [rime-pinyin-simp](https://github.com/rime/rime-pinyin-simp) | Apache-2.0 |

## Acknowledgements

- **Mai Vũ Tuyên** created and maintains [OpenKey](https://github.com/tuyenvm/OpenKey). If OpenKey is useful to you, consider [supporting the original author](https://tuyenvm.github.io/donate.html).
- Upstream contributors wrote the pull requests adapted here: **hungmtuci** (#333), **duyhnynh** (#332), **Quocker22** (#329), **luatnd** (#324), **uponatime2019** (#317), **nhutuananh** (#297), **kurokeita** (#289) and **quyleanh** (#287). Thanks also to everyone who has contributed to OpenKey over the years.
- The [RIME](https://rime.im) project provides the Chinese input engine and data.
