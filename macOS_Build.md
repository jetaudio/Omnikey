> English: see [BUILDING.md](BUILDING.md). Giới thiệu fork: [README.md](README.md).

## Cách build cho macOS:

### Build DMG bằng dòng lệnh hoặc GitHub Actions

Fork này có workflow **Build macOS DMG** trong tab **Actions**. Chọn **Run workflow** để tạo bản Release universal cho Apple Silicon (`arm64`) và Intel (`x86_64`). Tải artifact **Omnikey-macos-universal** sau khi workflow thành công; artifact gồm DMG, SHA-256 và thông tin commit/toolchain.

Để build trên máy có Xcode đầy đủ:

```bash
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer bash scripts/build-macos-dmg.sh
```

Kết quả nằm trong `dist/`. Bản tự build dùng chữ ký ad hoc, chưa notarize bằng tài khoản Apple Developer. Việc build và kiểm tra DMG không thay thế kiểm thử gõ tiếng Việt và cấp quyền Accessibility trên máy sử dụng.

Chế độ gõ tiếng Trung (中, Pinyin giản thể) dùng [librime](https://github.com/rime/librime) cùng dữ liệu `rime-prelude`, `rime-essay` và `rime-pinyin-simp`. Lần build đầu tiên cần mạng: bước "Embed Rime" của Xcode gọi `scripts/fetch-rime.sh` để tải đúng phiên bản đã ghim, kiểm tra SHA-256, biên dịch sẵn từ điển vào `Sources/Omnikey/macOS/ThirdParty/Rime` (không đưa vào git) rồi nhúng vào `Omnikey.app`. Giấy phép của các thành phần này nằm trong `Omnikey.app/Contents/Resources/Rime/licenses`.

Các kiểm thử hồi quy có thể chạy bằng Command Line Tools:

```bash
bash scripts/test-engine.sh
bash scripts/test-macos-events.sh
bash scripts/test-rime.sh
```

Để kiểm tra outlet, bố cục và thao tác Fn trên storyboard đã compile (máy cần có phiên đăng nhập đồ họa):

```bash
bash scripts/test-macos-ui.sh /path/to/Omnikey.app /tmp/omnikey-preferences.png
```

Lệnh UI QA dùng executable riêng và thay việc khởi tạo event tap bằng stub; không khởi chạy Omnikey hoặc gửi phím. Chi tiết review 8 PR và giới hạn kiểm chứng nằm trong [PR_REVIEW.md](PR_REVIEW.md).

Vì một số lý do mà Omnikey không thể đưa lêp Mac App Store được, nếu các bạn không yên tâm về file build chính thức từ tác giả, các bạn có thể tải mã nguồn về tự build lấy Omnikey cho mình. 

Yêu cầu:
- macOS 12 Monterey trở lên.
- Xcode 14 trở lên (đã kiểm tra với Xcode 27).

Tải mã nguồn từ dự án Omnikey trên GitHub về, mở dự án Omnikey bằng cách vào:
<img width="808" alt="Screen Shot 2022-06-01 at 13 34 39" src="https://user-images.githubusercontent.com/7700801/171342552-46c391a7-9160-4677-bb8a-9d8cececcfe6.png">

Dự án Omnikey sẽ hiện ra:
<img width="1522" alt="Screen Shot 2022-06-01 at 13 37 36" src="https://user-images.githubusercontent.com/7700801/171343050-aa2f9629-4670-4c1e-a3c0-0c25acefa42d.png">

Giờ bạn vào menu Product -> Archive để Xcode tiến hành build Omnikey ở dạng Production (có thể dùng được), sau khi build xong hộp thoại sẽ xuất hiện, bấm vào Distribute App
<img width="1112" alt="Screen Shot 2022-06-01 at 13 45 50" src="https://user-images.githubusercontent.com/7700801/171344111-540b66cb-556f-48cf-ad3b-0f44ab208d1a.png">

Nó sẽ ra:
<img width="1112" alt="Screen Shot 2022-06-01 at 13 48 53" src="https://user-images.githubusercontent.com/7700801/171344529-12eed723-4e66-4818-8502-fa693b269dba.png">

Bấm Next để chọn nơi lưu trữ file Build. Vậy là xong, bạn đã có ứng dụng Omnikey.app của riêng bạn.

Lưu ý: trong quá trình build, nếu XCode bão lỗi `invalid in C99`, khả năng cao là ở trong file `MJAccessibilityUtils.m`, dòng 26 `isEnabled = AXAPIEnabled();`, bạn lên đầu file thêm khai báo `extern BOOL AXAPIEnabled();` là sẽ build được.

Chúc các bạn Build Omnikey cho macOS thành công!
