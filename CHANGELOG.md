# Changelog

All notable changes to this fork are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

The fork is based on upstream [tuyenvm/OpenKey](https://github.com/tuyenvm/OpenKey)
`master` at `89c2fd3` (app version 2.0.4, build 48). Development builds are
identified by the source commit in their file name, for example
`OpenKey-2.0.5-<commit>-universal.dmg`. The upstream history is kept below.

## [Unreleased]

### Changed

- The project is renamed from OpenKey to **Omnikey**, and the repository moved to
  `jetaudio/Omnikey`. Source paths, project files, binaries (`Omnikey.app`,
  `Omnikey64.exe`, `ibus-omnikey`) and the macOS bundle ID
  (`io.github.jetaudio.omnikey`) follow the new name.
  - Windows copies settings from `HKCU\SOFTWARE\TuyenMai\OpenKey` to
    `HKCU\SOFTWARE\jetaudio\Omnikey` and removes the old `OpenKey` startup
    entries. Releases also ship `OpenKey-<version>-Windows.zip` with the old EXE
    names, so existing installs can update. The updater then keeps those names.
  - macOS copies the `com.tuyenmai.openkey` preferences once. Accessibility
    access must be granted again.

### Added

- Linux port for IBus (Ubuntu, GNOME, Wayland and X11), using the shared engine.
  - The settings app is built with GTK 4 and libadwaita.
  - Edit-in-place mode types without underlines and works in terminals. It
    sends real BackSpace keys through a `uinput` virtual keyboard and handles
    browser autocomplete and clicks elsewhere.
  - An underlined-preedit mode is also available.
  - `scripts/build-linux.sh` builds a `.deb` package.

### Fixed

- `ConvertTool.cpp` now includes `<algorithm>`, which GCC requires.

## [2.1.0] – 2026-10-07

Windows 2.1.0. macOS remains at 2.0.6 (build 50).

### Added

- Chinese Pinyin input on Windows through Rime 1.17.0, with bundled, checksummed
  dictionaries and persistent phrase learning.
- A compact horizontal candidate panel styled after macOS, with rounded translucent
  surfaces, soft shadows, blue selection, light/dark themes, DPI scaling and mouse paging.
- Complete x64/x86 Windows build and packaging scripts and regression tests for
  Pinyin conversion, key routing, mode cycling, focus changes and popup rendering.

### Changed

- The Windows mode hotkey cycles Vietnamese → English → Chinese → Vietnamese.
  Holding the shortcut switches only once until its key is released.
- Windows release ZIPs include Rime alongside the application, including both
  DLL architectures in the combined package.
- The Windows updater waits for archive extraction and installs the complete
  Rime payload and the matching application architecture. Its bundled helper
  runs from a temporary copy so it can also be updated.

### Fixed

- The Windows Alt+Z default now uses Windows key codes. Existing configurations
  containing the macOS default are repaired while preserving the beep setting.

## [2.0.6] – 2026-10-07

macOS 2.0.6 (build 50), Windows 2.0.6. First version that checks this fork for updates.

### Changed

- The update checker on macOS and Windows reads `version.json` from this
  repository, and the Windows updater downloads from its releases. The About
  and preferences links point to this repository.
- On macOS, accepting an update opens the latest release page. The app has no
  bundled update helper, so it used to quit without updating.
- The README defaults to Vietnamese, and the English version is `README.en.md`.
  The screenshot follows the light or dark GitHub theme.
- `scripts/test-macos-ui.sh` renders the dark appearance when
  `OPENKEY_UI_APPEARANCE=dark` is set.

## [2.0.5] – 2026-10-07

First release of the jetaudio fork. macOS 2.0.5 (build 49), Windows 2.0.5.

### Added

- **Chinese Pinyin input mode (中) on macOS**, powered by librime and the
  official `pinyin_simp` schema:
  - a candidate panel at the caret, with paging and mouse selection
  - phrase learning, and forgetting a learned phrase with Shift+Delete or Control+K
  - the mode shortcut now cycles Vietnamese → English → 中
- `scripts/fetch-rime.sh` downloads pinned, checksummed librime and Rime data
  and precompiles the dictionary. An Xcode build phase embeds them in the app.
- Globe/Fn key support for the mode-switch shortcut on macOS (upstream #324).
- Simple Telex 1 and 2 in the Windows settings and tray menu (upstream #289).
- Regression tests that run under AddressSanitizer/UndefinedBehaviorSanitizer:
  - engine: code-table conversion, smart-switch preferences, tone placement in
    old and modern orthography
  - macOS: event processing and event tap lifecycle, with system calls mocked
  - librime: Chinese input
  - a UI QA tool that checks the compiled storyboard and window layouts
- GitHub Actions workflows:
  - a universal (`arm64` + `x86_64`) macOS DMG
  - Windows x86/x64 builds with PE machine checks and build-provenance attestation
  - engine tests with MSVC and Windows key codes

### Changed

- The preferences, macro, convert-tool and About windows on macOS are redesigned
  in the macOS 26 (Tahoe) settings style. Storyboard outlets and actions are unchanged.
- On macOS, developer apps (Terminal and similar) start in English, but
  saved per-app choices and code tables are kept (upstream #333).
- macOS login items use `SMAppService` on macOS 13 and later (upstream #333).
- The minimum macOS version is now **12 Monterey**.
- Less work per keystroke:
  - the frontmost app and the input source are looked up once per key
  - the engine avoids repeated map lookups and copies
  - Chinese mode checks for an active composition without building the candidate list
- Repeated settings code in the macOS preferences and convert tool, and
  repeated `SendInput` code on Windows, is collapsed into shared helpers.

### Fixed

- macOS event tap: it is re-enabled after a timeout or user-input
  notification, with a 0.5 s watchdog. Modifier and composition state is reset
  (upstream #332, #333).
- Backspace events are allocated fresh for every replacement, and long strings
  are sent in complete UTF-16 chunks, including surrogate pairs (upstream #333).
- Spotlight: text is replaced in place only when the Spotlight field really has
  focus. Hidden or fading windows are ignored, and focus queries time out after
  20 ms (upstream #329).
- Fn combined with another key no longer switches the language when Fn is
  released (upstream #324).
- Windows: keyboard hooks are restored on session unlock. If installing a new
  hook fails, the old hook is kept (upstream #317).
- Windows: fixed a buffer overrun and a stray trailing character in the
  clipboard paste path.
- Windows: an empty key history no longer causes an out-of-range access on
  backspace.
- The convert tool keeps letter case when removing marks, and still decodes
  single-byte VNI/CP1258 characters (upstream #297).
- Packed smart-switch preferences keep their code-table bits (upstream #333).
- The Vietnamese engine starts a new word when entering or leaving Chinese mode,
  so an unfinished word does not join the next one.
- Tone placement no longer depends on platform key codes. In modern
  orthography, "iê"/"yê" was matched with a bitwise test that matched every
  letter on Windows. Placement on macOS is unchanged.
- The Windows artifact paths are fixed, and the app and updater are built for
  both architectures (upstream #287).
- The preferences title no longer overlaps the first row of controls.

### Removed

- Dead code in the Windows hook, the engine and the macOS app: unused globals,
  conditions that are always false, and unreachable code.

[Unreleased]: https://github.com/jetaudio/OpenKey/compare/v2.0.6...HEAD
[2.0.6]: https://github.com/jetaudio/OpenKey/compare/v2.0.5...v2.0.6
[2.0.5]: https://github.com/jetaudio/OpenKey/compare/89c2fd3...v2.0.5

---

# OpenKey Change Log (upstream)


##### OpenKey for Linux: (in development)

##### Version 1.2 RC5: (26/08/2019)
- Sửa lỗi không gõ được chữ "quởn".
- Không kiểm tra chính tả khi sử dụng dấu "[ ] { }".

##### Version 1.2 RC4: (24/08/2019)
- Bảng gõ tắt tiện lợi hơn, thêm tính năng Sửa từ.
- Cải thiện khả năng bỏ dấu, tốc độ nhanh hơn.
- Tự phục hồi dấu câu khi xóa ký tự (chữ “tuỳa” xóa “a” sẽ thành “tùy”,… )
- Sửa lỗi không gõ được từ “quét” khi bật chức năng tự phục hồi phím.
- Sửa lỗi ư và ơ khi gõ font Palatino trong MS Word.
- Sửa lỗi bảng mã VNI khi xóa ký tự, không thể gõ tiếng việt tiếp.

##### Version 1.2 RC3: (16/08/2019)
- Không gõ được "dui9, duoi96".
- Không gõ được "tuyps".

##### Version 1.2 RC2: (15/08/2019)
- Sửa lỗi không gõ được d i e u 9 6.
- Sửa lỗi không gõ tắt được khi dùng chế độ tiếng Anh với từ bắt đầu bằng.
- Sửa lỗi tự nhảy dấu khi gõ sai.
- Thêm thông tin phiên bản trong bảng giới thiệu.

##### Version 1.2 RC1: (13/08/2019)
- Chuyển chế độ thông minh: Bạn đang dùng chế độ gõ Tiếng Việt trên ứng dụng A, bạn chuyển qua ứng dụng B trước đó bạn dùng chế độ gõ Tiếng Anh, OpenKey sẽ tự động chuyển qua chế độ gõ Tiếng Anh cho bạn, khi bạn quay lại ứng dụng A, OpenKey tất nhiên sẽ chuyển lại chế độ gõ tiếng Việt, rất cơ động.
- Viết Hoa chữ cái đầu câu: Khi gõ văn bản dài, đôi khi bạn quên ghi hoa chữ cái đầu câu khi kết thúc một câu hoặc khi xuống hàng, tính năng này sẽ tự ghi hoa chữ cái đầu câu cho bạn, thật tuyệt vời.
Khôi phục phím với từ sai: hỗ trợ thêm các dấu ngắt câu như dấu chấm, phẩy,...
Sửa vài lỗi nho nhỏ khác.

##### Version 1.1 RC: (12/08/2019)
- Chế độ “Gửi từng phím”: OpenKey bản mới (1.1) mặc định dùng kỹ thuật mới gửi dữ liệu 1 lần thay vì gửi nhiều lần cho chuỗi ký tự, nên nếu có ứng dụng nào không tương thích, hãy bật tính năng này lên, mặc định thì nên tắt vì kỹ thuật mới sẽ chạy nhanh hơn.
- Phục hồi phím với từ sai.
- Nâng cao khả năng check chính tả.
- Sửa lỗi thanh địa chỉ trình duyệt (on/off).
- Bỏ tính năng "cho phép bỏ dấu tự do".
- Gõ tắt: bao gồm bật/tắt, bảng soạn các từ gõ tắt, hỗ trợ ký tự bất kỳ, bảng mã bất kỳ. Khi soạn thảo các từ gõ tắt, bạn phải nhập ở bảng mã Unicode dựng sẵn.
- Gõ tắt ngay khi trong chế độ gõ tiếng Anh (on/off).
- Sửa lỗi trên một số phần mềm.

##### Version 1.0.20: (06/08/2019)
- Sửa lỗi phím tắt chuyển chế độ, phím tắt của ứng dụng khác vẫn hoạt động nếu bị trùng.
- Cho phép gõ “Đ” ngay sau phụ âm.
- Không hiện Icon trên thanh Dock mục recent app.
- Sửa lỗi “oăc” ra “ooạc” trong kiểu gõ VNI.
- Sửa vài lỗi nhỏ xíu khác.

##### Version 1.0.19: (04/08/2019)
- Sửa lỗi không gõ được chữ “gì” khi dùng bỏ dấu kiểu cũ.
- Sửa lỗi không gõ được Unicode tổ hợp trên ứng dụng Stickies.
- Sửa lỗi gõ các âm "oong, ooc".

##### Version 1.0.18: (01/08/2019)
- Không sử dụng w -> ư trong Simple Telex.
- Bật tắt kêu beep khi chuyển chế độ.
- Thêm phím chuyển Shift, giờ có thể sử dụng Ctrl + Shift hoặc Command + Shift.
- Sửa vài lỗi khác.
- Hỗ trợ cho macOS bản cũ.

##### Version 1.0.17: (31/07/2019)
- Add Simple Telex mode.
- Black/White icon on menu bar.
- Space and back key improved.
- Modern orthography.
- Custom switch key.
- Quick telex (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th).
- Support TextWrangler.

##### Version 1.0.14: (09/04/2019)
- Add case "uýt".
- Improve typing English in Vietnamese mode.

##### Version 1.0.11: (27/02/2019)
- Add case "chú thòong", "gòong".

##### Version 1.0.10: (26/02/2019)
- Fix case "duocd".

##### Version 1.0.9: (22/02/2019)
- Fix incorrect word when switch language without pressing Space key.

##### Version 1.0.8: (19/02/2019)
- Switch key: Control + Command + Space  --> Control + Z

##### Version 1.0.7: (15/02/2019)
- Fix case "duongd".
- Fix end consonant "t".

##### Version 1.0.6: (15/02/2019)
- Fix case "quatw".

##### Version 1.0.5: (13/02/2019)
- Spelling enhanced.
- Correct 1x menu icon.

##### Version 1.0.3: (11/02/2019)
- Fix auto correct on Chrome.

##### Version 1.0: (11/02/2019)
- First release.



# OpenKey lịch sử

##### Version 1.0.17: (31/07/2019)
- Thêm chế độ Simple Telex.
- Icon trắng đen trên menu bar.
- Lỡ bấm phím Space, xoá space vẫn có thể bỏ dấu.
- Bỏ dấu kiểu cũ/mới: òa, úy | oà, uý.
- Tuỳ chọn phím chuyển.
- Gõ nhanh (cc=ch, gg=gi, kk=kh, nn=ng, qq=qu, pp=ph, tt=th).
- Hỗ trợ TextWrangler.

##### Version 1.0.11: (27/02/2019)
- Thêm trường hợp "chú thòong", "gòong".

##### Version 1.0.10: (26/02/2019)
- Sửa lỗi "duocd".

##### Version 1.0.9: (22/02/2019)
- Sửa lỗi từ sai khi đổi chế độ mà không bấm phím Space.

##### Version 1.0.8: (19/02/2019)
- Phím chuyển: Control + Command + Space  --> Control + Z

##### Version 1.0.7: (15/02/2019)
- Sửa lỗi "duongd".
- Sửa lỗi phụ âm cuối "t".

##### Version 1.0.6: (15/02/2019)
- Sửa lỗi "quatw".

##### Version 1.0.5: (13/02/2019)
- Nâng cao chính tả.
- Sửa icon cho màn hình non-retina.

##### Version 1.0.3: (11/02/2019)
- Sửa lỗi thanh địa chỉ trên Chrome.

##### Version 1.0: (11/02/2019)
- Phát hành lần đầu.
