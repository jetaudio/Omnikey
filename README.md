# Omnikey

[![Build macOS DMG](https://github.com/jetaudio/Omnikey/actions/workflows/macos-dmg.yml/badge.svg)](https://github.com/jetaudio/Omnikey/actions/workflows/macos-dmg.yml)
[![MSBuild](https://github.com/jetaudio/Omnikey/actions/workflows/msbuild.yml/badge.svg)](https://github.com/jetaudio/Omnikey/actions/workflows/msbuild.yml)
[![Build Linux DEB](https://github.com/jetaudio/Omnikey/actions/workflows/linux-deb.yml/badge.svg)](https://github.com/jetaudio/Omnikey/actions/workflows/linux-deb.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

**Một bộ gõ tiếng Việt nguồn mở, dùng chung một engine trên macOS, Windows và Linux (Ubuntu).**

## Vì sao có fork này

Mình đã quá mệt mỏi với các bộ gõ tiếng Việt trên Ubuntu và macOS: chỗ thì gạch chân, chỗ thì mất chữ, sang terminal lại gõ sai, mỗi máy phải học một bộ gõ khác nhau. Trong khi đó mình lại quá nghiền UniKey trên Windows: gõ đâu ra đó, không gạch chân, không phải nghĩ.

Vì vậy mình fork OpenKey và cải tiến nó, để **một bộ gõ, một cách gõ, dùng được trên mọi nền tảng**, với cảm giác gõ giống UniKey nhất có thể.

## Nguồn gốc

Omnikey trước đây có tên **OpenKey (bản fork của jetaudio)**, repo cũ `jetaudio/OpenKey` tự chuyển hướng sang repo này.
Khi chuyển từ OpenKey sang Omnikey:
- **Windows:** bản OpenKey cũ vẫn tự cập nhật lên Omnikey. Cài đặt được chép sang tự động, và mục khởi động cùng Windows cũ được dọn đi.
- **macOS:** cài đặt được chép sang tự động. Vì app đổi tên, cần **cấp lại quyền Trợ năng** cho Omnikey và xoá `OpenKey.app` cũ để hai bộ gõ không chạy cùng lúc.
- **Linux:** gói `ibus-omnikey` thay gói `ibus-openkey`. Cài đặt và gõ tắt được chép sang; mở app Omnikey một lần để nguồn nhập cũ chuyển sang Omnikey.

Đây là bản fork được duy trì của [OpenKey](https://github.com/tuyenvm/OpenKey) do Mai Vũ Tuyên phát triển.
Fork dựa trên `master` của bản gốc tại commit [`89c2fd3`](https://github.com/tuyenvm/OpenKey/commit/89c2fd3bf258562f2349f89b49d81e2f140c3fc3).
Fork bổ sung bản Linux (IBus) mới, chế độ gõ tiếng Trung, giao diện macOS mới, các bản sửa lỗi lấy từ những pull request còn mở ở repo gốc, cùng bộ kiểm thử hồi quy và CI build tự động.

[English](README.en.md) · [Lịch sử thay đổi](CHANGELOG.md) · [Hướng dẫn build](BUILDING.md) · [Review PR gốc](PR_REVIEW.md)

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/images/preferences-dark.png">
  <img alt="Bảng điều khiển Omnikey trên macOS" src="docs/images/preferences-light.png">
</picture>

## Omnikey có thêm gì so với OpenKey gốc

### Tính năng mới

- **Bản Linux mới (Ubuntu, GNOME, Wayland và X11):** chạy trên IBus và dùng chung engine với macOS và Windows.
  - **Gõ kiểu UniKey, không gạch chân:** dùng phím Backspace thật qua bàn phím ảo `uinput`, nên gõ được cả trong terminal. Với ứng dụng hỗ trợ, bộ gõ sửa chữ trực tiếp.
  - Vẫn có chế độ gạch chân từ đang gõ cho ai thích kiểu ổn định truyền thống.
  - Tự xử lý ô có gợi ý tự hoàn thành (thanh địa chỉ trình duyệt) và nhận biết khi bạn click sang chỗ khác.
  - App cài đặt GTK 4/libadwaita theo phong cách Ubuntu mới nhất; đổi cài đặt là có hiệu lực ngay.
  - Phím chuyển Việt/Anh: Ctrl+Shift (mặc định), Ctrl+Space hoặc Alt+Z. Chữ **V/E** hiện trên thanh trạng thái.
- **Gõ tiếng Trung (macOS và Windows):** thêm chế độ thứ ba, **中**, để gõ Pinyin giản thể.
  - Dùng [librime](https://github.com/rime/librime) với bộ `pinyin_simp` chính thức.
  - Bảng ứng viên hiện ngay cạnh con trỏ.
  - Bộ gõ tự học cụm từ. **Shift+Delete** hoặc **Control+K** xóa một cụm đã học.
  - Phím tắt chuyển chế độ xoay vòng **Tiếng Việt → English → 中**.
- **Giao diện macOS mới:** bảng điều khiển, gõ tắt, chuyển mã và giới thiệu được thiết kế lại theo phong cách Cài đặt của macOS 26 (Tahoe).
- **Phím 🌐/Fn (macOS):** dùng được phím Fn riêng lẻ hoặc kết hợp với phím khác để chuyển chế độ.
- **Simple Telex 2 trên Windows:** có trong bảng điều khiển và menu khay hệ thống.

### Sửa lỗi độ ổn định

Các bản sửa này được tích hợp từ 8 PR còn mở ở repo gốc: #287, #289, #297, #317, #324, #329, #332 và #333. Mỗi PR đều được review, viết lại một phần và có kiểm thử. Chi tiết nằm trong [PR_REVIEW.md](PR_REVIEW.md).

- **Event tap (macOS):** tự bật lại khi macOS tắt nó do timeout, có watchdog 0,5 giây.
- **Gửi phím (macOS):** mỗi lần thay chữ tạo cặp sự kiện Backspace mới, và chuỗi dài được gửi theo từng khối UTF-16 trọn vẹn.
- **Spotlight:** chỉ thay chữ tại chỗ khi ô Spotlight thật sự đang nhận phím.
- **Khởi động cùng máy:** dùng `SMAppService` trên macOS 13 trở lên.
- **Ứng dụng lập trình:** Terminal và các ứng dụng tương tự mặc định gõ tiếng Anh, nhưng vẫn giữ lựa chọn và bảng mã người dùng đã lưu.
- **Windows:** tự cài lại hook bàn phím khi mở khóa máy, và sửa lỗi tràn bộ đệm cùng ký tự thừa khi dán qua clipboard.
- **Công cụ chuyển mã:** giữ chữ hoa/thường khi bỏ dấu, và vẫn giải mã đúng ký tự một byte của VNI/CP1258.
- **Chuyển chế độ:** bộ gõ tiếng Việt bắt đầu từ mới mỗi khi vào hoặc ra chế độ tiếng Trung.

### Kỹ thuật

- **Kiểm thử:** chạy với AddressSanitizer/UndefinedBehaviorSanitizer.
  - Engine có hơn 3.400 assertion, gồm cả cách đặt dấu kiểu cũ và kiểu mới.
  - Phía macOS có kiểm thử xử lý sự kiện phím, vòng đời event tap và gõ tiếng Trung.
- **CI:** GitHub Actions build DMG universal (`arm64` + `x86_64`) và file EXE Windows x86/x64.
- **Hiệu năng:** giảm khối lượng xử lý cho mỗi lần gõ phím.

## Tính năng kế thừa từ OpenKey

- **Kiểu gõ:** Telex, VNI, Simple Telex 1, Simple Telex 2.
- **Bảng mã:** Unicode dựng sẵn, TCVN3 (ABC), VNI Windows, Unicode tổ hợp, Vietnamese Locale CP1258.
- **Cách gõ:**
  - Bỏ dấu kiểu mới (`oà`, `uý`) hoặc kiểu cũ (`òa`, `úy`).
  - Kiểm tra chính tả, và tự khôi phục phím khi gõ sai từ.
  - Gõ nhanh: `cc`=ch, `gg`=gi, `kk`=kh, `nn`=ng, `qq`=qu, `pp`=ph, `tt`=th.
  - Gõ tắt phụ âm đầu (f→ph, j→gi, w→qu) và phụ âm cuối (g→ng, h→nh, k→ch).
- **Gõ tắt (macro):** không giới hạn độ dài, nhập và xuất được ra file.
- **Chuyển chế độ thông minh:** tự nhớ chế độ gõ và bảng mã theo từng ứng dụng.
- **Tạm tắt:** giữ Ctrl để tạm tắt kiểm tra chính tả, giữ Cmd/Alt để tạm tắt Omnikey.
- **Viết hoa:** tự viết hoa chữ cái đầu câu.
- **Công cụ chuyển mã:** chuyển văn bản giữa các bảng mã và đổi chữ hoa/thường, có phím tắt riêng.
- **Sửa lỗi gợi ý:** sửa lỗi tự hoàn thành trên trình duyệt và Excel.
- **Ngôn ngữ khác:** tùy chọn tắt tiếng Việt khi bộ gõ hệ thống không phải tiếng Anh.

## Yêu cầu hệ thống

- macOS 12 Monterey trở lên (Apple Silicon hoặc Intel).
- Windows Vista trở lên (x86 hoặc x64).
- Linux có IBus và libadwaita 1.5 trở lên. Đã thử trên Ubuntu 26.04 (GNOME 50, Wayland).

## Cài đặt

**macOS**

1. Tải `Omnikey-<phiên bản>-macOS-universal.dmg` từ [bản phát hành mới nhất](https://github.com/jetaudio/Omnikey/releases/latest). Bản dựng thử nằm ở artifact **Omnikey-macos-universal** của workflow [Build macOS DMG](https://github.com/jetaudio/Omnikey/actions/workflows/macos-dmg.yml). Bạn cũng có thể [tự build](BUILDING.md).
2. Mở DMG và kéo **Omnikey.app** vào **Applications**.
3. Bản build được ký ad hoc và chưa notarize. Lần đầu mở, hãy bấm chuột phải vào app rồi chọn **Open**, hoặc cho phép trong **Cài đặt Hệ thống → Quyền riêng tư & Bảo mật**.
4. Cấp quyền tại **Cài đặt Hệ thống → Quyền riêng tư & Bảo mật → Trợ năng**. Không tắt quyền này khi đang dùng Omnikey.

**Windows**

1. Tải `Omnikey-<phiên bản>-Windows.zip` từ [bản phát hành mới nhất](https://github.com/jetaudio/Omnikey/releases/latest), hoặc artifact **Omnikey** của workflow [MSBuild](https://github.com/jetaudio/Omnikey/actions/workflows/msbuild.yml).
2. Giải nén toàn bộ, giữ thư mục `Rime` cạnh EXE, rồi chạy `Omnikey64.exe` trên Windows 64-bit hoặc `Omnikey32.exe` trên Windows 32-bit. Có thêm ZIP riêng cho x64/x86. Bản 2.0.5 để EXE trong thư mục `x64/` và `x86/`.
3. Bấm đồng ý khi Windows hỏi quyền quản trị.

**Linux (Ubuntu)**

1. Build gói `.deb`. Xem phần [Build và kiểm thử](#build-và-kiểm-thử).
2. Cài gói rồi khởi động lại IBus:
   ```bash
   sudo apt install ./dist/ibus-omnikey_*_amd64.deb
   ibus restart
   ```
3. Mở app **Omnikey** và bấm **Thêm Omnikey**. Hoặc vào **Cài đặt → Bàn phím → Nguồn nhập → + → Tiếng Việt → Omnikey**.
4. Muốn gõ không gạch chân như UniKey: trong app, vào **Gõ phím → Sửa trực tiếp**.

> [!IMPORTANT]
> Hãy tắt các bộ gõ tiếng Việt khác khi dùng Omnikey, vì hai bộ gõ chạy cùng lúc sẽ xung đột với nhau.

## Hướng dẫn nhanh

- **Chuyển chế độ:** phím tắt xoay vòng **V → E → 中 → V** trên macOS và Windows; Windows mặc định **Alt+Z**. Trên Linux chỉ có **V ↔ E**, mặc định **Ctrl+Shift**. Có thể chọn trực tiếp trên menu bar hay khay hệ thống.
- **Dùng phím 🌐 trên macOS:** vào **Cài đặt Hệ thống → Bàn phím** và đặt "Nhấn phím 🌐" thành **Không làm gì**.
- **Chế độ 中:**
  - Gõ Pinyin.
  - **Space** chọn ứng viên đầu tiên, phím **1–7** hoặc bấm chuột để chọn ứng viên khác.
  - **−/=** hoặc **Page Up/Page Down** để lật trang.
  - **Esc** để hủy.
  - Trên macOS, bật **Caps Lock** để gõ chữ Latin.

## Build và kiểm thử

Xem [BUILDING.md](BUILDING.md). Ngắn gọn như sau:

```bash
bash scripts/build-macos-dmg.sh    # build DMG vào dist/ (cần Xcode đầy đủ)
bash scripts/test-engine.sh        # kiểm thử engine gõ
bash scripts/test-macos-events.sh  # kiểm thử xử lý phím trên macOS
bash scripts/test-macos-updater.sh # kiểm thử xác minh, cài và khôi phục cập nhật
bash scripts/test-rime.sh          # kiểm thử gõ tiếng Trung
bash scripts/build-linux.sh        # Linux: kiểm thử engine, build và đóng gói .deb vào dist/
```

Trên Ubuntu, cần cài trước các gói sau:

```bash
sudo apt install build-essential cmake pkg-config libibus-1.0-dev libadwaita-1-dev
```

## Hạn chế đã biết

- **Chữ ký:** bản build chỉ ký ad hoc và chưa notarize.
- **Cập nhật tự động:** từ bản 2.0.6, app kiểm tra bản mới trên repo này. Bản 2.0.5 vẫn đọc `version.json` của repo gốc, nên sẽ không tự báo có bản mới; hãy tải 2.0.6 từ trang Releases. Trên macOS, bản có updater mới sẽ tải, xác minh và cài DMG sau khi bấm "Có", rồi tự mở lại. Bản chưa có updater cần cài thủ công một lần. Release cần có DMG universal và file `.dmg.sha256`; ứng dụng phải nằm trong thư mục có quyền ghi. Updater Windows chỉ tự thay `Omnikey64.exe`.
- **Chưa kiểm thử tương tác lâu dài:** các tình huống sau chưa được thử bằng tay trong thời gian dài. Kiểm thử tự động chỉ dùng mock cho các tương tác hệ thống này.
  - gõ trong Apple Mail, WebKit và Spotlight
  - sleep/wake
  - Login Items
  - khóa/mở khóa trên Windows
- **Tiếng Trung:** chỉ có trên macOS và Windows, và chỉ hỗ trợ Pinyin giản thể. Bản Linux chưa có.
- **Linux:**
  - Bản Linux còn mới, mới được thử trên Ubuntu 26.04 với GNOME, Firefox, LibreOffice và Terminal.
  - Chế độ "Sửa trực tiếp" cần quyền dùng `/dev/uinput`. Gói cài thêm một quy tắc udev cấp quyền này cho người đang đăng nhập. Nhờ vậy mọi chương trình chạy dưới tài khoản đó cũng giả lập được phím bấm.
  - Ứng dụng không báo vị trí con trỏ cho IBus (ví dụ terminal) không cho bộ gõ biết khi bạn click chuột sang chỗ khác giữa chừng một từ.
  - Kiểu gõ dựa trên vị trí phím QWERTY.

## Đóng góp

Rất hoan nghênh issue và pull request. Hãy tạo nhánh từ `master`, giữ thay đổi gọn gàng, thêm kiểm thử trong `tests/` khi thay đổi hành vi, và chạy các script kiểm thử trước khi gửi PR. Các bản sửa cho engine dùng chung cũng có thể gửi về [repo gốc](https://github.com/tuyenvm/OpenKey).

## Giấy phép

Omnikey là phần mềm tự do theo giấy phép [GNU GPL v3.0](LICENSE). Đúng như giấy phép yêu cầu, bản fork này vẫn là mã nguồn mở và ghi rõ bản gốc là OpenKey.

App macOS có kèm các thành phần sau:

- librime (BSD 3-Clause)
- rime-prelude và rime-essay (LGPL-3.0)
- rime-pinyin-simp (Apache-2.0)

Văn bản giấy phép nằm trong `Omnikey.app/Contents/Resources/Rime/licenses`.

## Ghi nhận

- **Mai Vũ Tuyên** là tác giả và người duy trì [OpenKey](https://github.com/tuyenvm/OpenKey). Bạn có thể [ủng hộ tác giả gốc tại đây](https://tuyenvm.github.io/donate.html).
- Các bản sửa lỗi được tích hợp từ PR của: hungmtuci, duyhnynh, Quocker22, luatnd, uponatime2019, nhutuananh, kurokeita và quyleanh.
- Dự án [RIME](https://rime.im) cung cấp engine và dữ liệu gõ tiếng Trung.
