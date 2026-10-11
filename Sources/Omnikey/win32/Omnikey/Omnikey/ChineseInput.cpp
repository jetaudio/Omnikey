#include "stdafx.h"
#include "WindowsRime.h"
#include "AppDelegate.h"
#include "CandidatePanel.h"
#include <shlobj.h>
#include <thread>
#include <memory>
#include <algorithm>

namespace ChineseInput {
namespace {
    constexpr UINT WM_RIME_READY = WM_APP + 71;
    constexpr UINT WM_RIME_KEY = WM_APP + 72;
    HWND panel = nullptr, targetWindow = nullptr;
    std::unique_ptr<WindowsRime> engine;
    std::thread loader;
    bool loaded = false, composing = false;
    bool swallowed[256] = {};
    unsigned generation = 0;
    unsigned queuedKeys = 0;
    unsigned dpi = 96;
    int hover = -1;
    bool shown = false, dark = false;
    RECT caret = {};
    POINT panelPosition = {};
    CandidatePanel::Layout layout;
    ChineseComposition current;
    std::wstring failure;
    struct PendingKey { int keysym, mask; unsigned generation; HWND target; DWORD virtualKey, flags; };

    bool sendText(const std::wstring& text) {
        std::vector<INPUT> events;
        for (wchar_t ch : text) {
            INPUT down = {};
            down.type = INPUT_KEYBOARD;
            down.ki.wScan = ch;
            down.ki.dwFlags = KEYEVENTF_UNICODE;
            down.ki.dwExtraInfo = 1; // Omnikey's own event marker.
            events.push_back(down);
            down.ki.dwFlags |= KEYEVENTF_KEYUP;
            events.push_back(down);
        }
        if (events.empty()) return true;
        return SendInput((UINT)events.size(), events.data(), sizeof(INPUT)) == events.size();
    }

    void forwardKey(const PendingKey& key) {
        INPUT events[2] = {};
        events[0].type = events[1].type = INPUT_KEYBOARD;
        events[0].ki.wVk = events[1].ki.wVk = (WORD)key.virtualKey;
        events[0].ki.dwExtraInfo = events[1].ki.dwExtraInfo = 1;
        events[0].ki.dwFlags = (key.flags & LLKHF_EXTENDED) ? KEYEVENTF_EXTENDEDKEY : 0;
        events[1].ki.dwFlags = events[0].ki.dwFlags | KEYEVENTF_KEYUP;
        SendInput(2, events, sizeof(INPUT));
    }

    void repaint() {
        HBITMAP bitmap = CandidatePanel::render(current, layout, dark, hover);
        if (!bitmap) return;
        HDC screen = GetDC(nullptr), memory = CreateCompatibleDC(screen);
        HGDIOBJ previous = SelectObject(memory, bitmap);
        SIZE size = { layout.width, layout.height };
        POINT origin = {};
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(panel, screen, &panelPosition, &size, memory, &origin, 0, &blend, ULW_ALPHA);
        SelectObject(memory, previous); DeleteObject(bitmap);
        DeleteDC(memory); ReleaseDC(nullptr, screen);
    }

    void showComposition() {
        current = engine->composition();
        composing = current.composing() || queuedKeys != 0;
        if (!current.composing()) {
            ShowWindow(panel, SW_HIDE);
            shown = false;
            return;
        }
        if (!shown) {
            GUITHREADINFO info = { sizeof(info) };
            if (GetGUIThreadInfo(GetWindowThreadProcessId(targetWindow, nullptr), &info) && info.hwndCaret) {
                POINT top = { info.rcCaret.left, info.rcCaret.top }, bottom = { info.rcCaret.right, info.rcCaret.bottom };
                ClientToScreen(info.hwndCaret, &top); ClientToScreen(info.hwndCaret, &bottom);
                caret = { top.x, top.y, bottom.x, bottom.y };
            } else {
                POINT mouse; GetCursorPos(&mouse);
                caret = { mouse.x, mouse.y, mouse.x + 1, mouse.y + 20 };
            }
            auto getDpi = reinterpret_cast<UINT(WINAPI*)(HWND)>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
            UINT targetDpi = getDpi ? getDpi(targetWindow) : 0;
            if (targetDpi) dpi = targetDpi;
            dark = CandidatePanel::darkAppearance();
            hover = -1;
        }
        RECT work = {};
        MONITORINFO monitor = { sizeof(monitor) };
        POINT point = { caret.left, caret.bottom };
        if (GetMonitorInfo(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor)) work = monitor.rcWork;
        layout = CandidatePanel::measure(current, dpi, work.right - work.left - 8);
        int shadow = layout.surface.top, gap = MulDiv(6, dpi, 96);
        point.x -= shadow + MulDiv(8, dpi, 96);
        point.y += gap - shadow;
        if (point.y + layout.height - shadow > work.bottom) point.y = caret.top - gap - layout.height + shadow;
        point.x = (std::max)(work.left, (std::min)(point.x, work.right - layout.width));
        point.y = (std::max)(work.top, (std::min)(point.y, work.bottom - layout.height));
        panelPosition = point;
        SetWindowPos(panel, HWND_TOPMOST, point.x, point.y, layout.width, layout.height, SWP_NOACTIVATE);
        repaint();
        ShowWindow(panel, SW_SHOWNOACTIVATE);
        shown = true;
    }

    void finishKey() {
        std::wstring commit = engine->takeCommit();
        if (!commit.empty() && targetWindow == GetForegroundWindow() && !sendText(commit)) {
            // Retain the text for recovery if the target rejects SendInput (e.g. elevated app).
            if (OpenClipboard(panel)) {
                HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (commit.size() + 1) * sizeof(wchar_t));
                if (memory) {
                    void* destination = GlobalLock(memory);
                    if (destination) {
                        memcpy(destination, commit.c_str(), (commit.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(memory);
                        EmptyClipboard();
                        if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
                    } else GlobalFree(memory);
                }
                CloseClipboard();
            }
            reset();
            MessageBoxW(nullptr, L"Ứng dụng đích không nhận chữ. Đã chép chữ vào clipboard; bạn có thể dán bằng Ctrl+V.", L"Omnikey Pinyin", MB_OK | MB_ICONINFORMATION);
            return;
        }
        showComposition();
    }

    LRESULT CALLBACK panelProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
        switch (message) {
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_RIME_READY:
            if (loader.joinable()) loader.join();
            loaded = wParam != 0;
            if (!loaded) {
                failure = engine->error();
                if (vChineseMode) {
                    AppDelegate::getInstance()->selectInputMode(1);
                    MessageBoxW(nullptr, failure.c_str(), L"Omnikey Pinyin", MB_OK | MB_ICONINFORMATION);
                }
            }
            return 0;
        case WM_RIME_KEY: {
            std::unique_ptr<PendingKey> key(reinterpret_cast<PendingKey*>(lParam));
            if (key->generation != generation) return 0;
            if (queuedKeys) --queuedKeys;
            if (!loaded || !vChineseMode || key->target != GetForegroundWindow()) { reset(); return 0; }
            targetWindow = key->target;
            bool handled = engine->process(key->keysym, key->mask);
            finishKey();
            if (!handled && key->target == GetForegroundWindow()) {
                if (key->keysym >= 0x20 && key->keysym < 0x7f) sendText(std::wstring(1, (wchar_t)key->keysym));
                else forwardKey(*key);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            int index = CandidatePanel::hitTest(layout, { (short)LOWORD(lParam), (short)HIWORD(lParam) });
            if (loaded && index >= 0 && index < (int)current.candidates.size() && targetWindow == GetForegroundWindow()) {
                engine->select(index);
                finishKey();
            } else if (loaded && targetWindow == GetForegroundWindow() &&
                       ((index == -2 && current.page > 0) || (index == -3 && !current.lastPage))) {
                engine->process(index == -2 ? 0xff55 : 0xff56);
                finishKey();
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            int hit = CandidatePanel::hitTest(layout, { (short)LOWORD(lParam), (short)HIWORD(lParam) });
            if (hit != hover) { hover = hit; repaint(); }
            TRACKMOUSEEVENT tracking = { sizeof(tracking), TME_LEAVE, window, 0 };
            TrackMouseEvent(&tracking);
            SetCursor(LoadCursor(nullptr, hit != -1 ? IDC_HAND : IDC_ARROW));
            return 0;
        }
        case WM_MOUSELEAVE: hover = -1; if (shown) repaint(); return 0;
        case WM_SETTINGCHANGE: dark = CandidatePanel::darkAppearance(); if (shown) repaint(); return 0;
        case WM_NCHITTEST: {
            POINT point = { (short)LOWORD(lParam), (short)HIWORD(lParam) }; ScreenToClient(window, &point);
            return PtInRect(&layout.surface, point) ? HTCLIENT : HTTRANSPARENT;
        }
        case WM_PAINT: ValidateRect(window, nullptr); return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

void start() {
    if (panel) return;
    WNDCLASSW cls = {};
    cls.lpfnWndProc = panelProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"OmnikeyChineseCandidates";
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&cls);
    panel = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_LAYERED,
        cls.lpszClassName, L"Omnikey Pinyin", WS_POPUP,
        0, 0, 200, 80, nullptr, nullptr, cls.hInstance, nullptr);
    if (!panel) { failure = L"Không tạo được cửa sổ ứng viên."; return; }
    HDC dc = GetDC(panel);
    dpi = GetDeviceCaps(dc, LOGPIXELSY); ReleaseDC(panel, dc);
    wchar_t executable[MAX_PATH]; GetModuleFileNameW(nullptr, executable, MAX_PATH);
    std::wstring bundle(executable);
    std::wstring directory = bundle.substr(0, bundle.find_last_of(L"\\/"));
    bundle = directory + L"\\Rime";
    // The 2.0.6 updater moves only the EXE but leaves bundled files in its staging
    // folder. Read those files so a first upgrade still has working Chinese input.
    std::wstring legacy = directory + L"\\_OmnikeyUpdate\\Rime";
    if (GetFileAttributesW((bundle + L"\\build\\pinyin_simp.table.bin").c_str()) == INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((legacy + L"\\build\\pinyin_simp.table.bin").c_str()) != INVALID_FILE_ATTRIBUTES)
        bundle = legacy;
    PWSTR localData = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localData))) {
        failure = L"Không tìm được thư mục dữ liệu người dùng.";
        return;
    }
    std::wstring user = std::wstring(localData) + L"\\Omnikey\\Rime";
    CoTaskMemFree(localData);
    engine.reset(new WindowsRime());
    loader = std::thread([bundle, user] {
        bool result = engine->initialize(bundle, user);
        PostMessageW(panel, WM_RIME_READY, result, 0);
    });
}

void stop() {
    if (loader.joinable()) loader.join();
    reset();
    // Release unprocessed queued events before destroying their destination window.
    MSG message;
    while (PeekMessageW(&message, panel, WM_RIME_KEY, WM_RIME_KEY, PM_REMOVE))
        delete reinterpret_cast<PendingKey*>(message.lParam);
    engine.reset();
    if (panel) DestroyWindow(panel);
    panel = nullptr;
    loaded = false;
}

bool ready() { return loaded; }
std::wstring status() { return failure.empty() ? L"Đang chuẩn bị bộ gõ tiếng Trung, hãy thử lại sau vài giây." : failure; }
bool containsWindow(HWND window) { return panel && window == panel; }

void reset() {
    ++generation;
    queuedKeys = 0;
    composing = false;
    if (loaded) engine->clear();
    current = {};
    shown = false;
    if (panel) ShowWindow(panel, SW_HIDE);
}

bool handleKey(WPARAM message, const KBDLLHOOKSTRUCT& key, unsigned modifiers) {
    unsigned vk = key.vkCode;
    if (vk >= 256) return false;
    bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    if (!down) {
        bool result = swallowed[vk];
        swallowed[vk] = false;
        return result;
    }
    if (!vChineseMode || !loaded) return false;
    DWORD targetProcess = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &targetProcess);
    if (targetProcess == GetCurrentProcessId()) { reset(); return false; }
    // Ctrl/Alt/Win shortcuts always reach the target application.
    if (modifiers & (0x02 | 0x04 | 0x20)) { reset(); return false; }
    int keysym = 0, mask = 0;
    bool shift = (modifiers & 0x01) != 0;
    switch (vk) {
    case VK_BACK: keysym = 0xff08; break;
    case VK_DELETE: keysym = 0xffff; mask = shift ? 1 : 0; break;
    case VK_RETURN: keysym = 0xff0d; break;
    case VK_ESCAPE: keysym = 0xff1b; break;
    case VK_TAB: keysym = 0xff09; mask = shift ? 1 : 0; break;
    case VK_LEFT: keysym = 0xff51; break;
    case VK_UP: keysym = 0xff52; break;
    case VK_RIGHT: keysym = 0xff53; break;
    case VK_DOWN: keysym = 0xff54; break;
    case VK_PRIOR: keysym = 0xff55; break;
    case VK_NEXT: keysym = 0xff56; break;
    case VK_HOME: keysym = 0xff50; break;
    case VK_END: keysym = 0xff57; break;
    case VK_SPACE: keysym = 0x20; break;
    default: {
        BYTE keyboard[256] = {};
        keyboard[VK_SHIFT] = shift ? 0x80 : 0;
        keyboard[VK_CAPITAL] = (GetKeyState(VK_CAPITAL) & 1) ? 1 : 0;
        wchar_t text[4] = {};
        HWND target = GetForegroundWindow();
        HKL layout = GetKeyboardLayout(GetWindowThreadProcessId(target, nullptr));
        // Flag 4 prevents ToUnicodeEx from changing the target's dead-key state.
        int count = ToUnicodeEx(vk, key.scanCode, keyboard, text, 4, 4, layout);
        if (count == 1 && text[0] >= 0x21 && text[0] < 0x7f) keysym = text[0];
    }
    }
    if (!keysym) { reset(); return false; }
    bool letter = (keysym >= 'a' && keysym <= 'z') || (keysym >= 'A' && keysym <= 'Z');
    if (!composing && !letter) return false;
    auto pending = new PendingKey { keysym, mask, generation, GetForegroundWindow(), key.vkCode, key.flags };
    if (!PostMessageW(panel, WM_RIME_KEY, 0, reinterpret_cast<LPARAM>(pending))) { delete pending; return false; }
    ++queuedKeys;
    composing = true; // Include queued letters when deciding whether Space belongs to Rime.
    swallowed[vk] = true;
    return true;
}
}
