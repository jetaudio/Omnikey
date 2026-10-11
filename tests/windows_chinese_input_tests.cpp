// Exercise the real Windows input queue/panel code. Only desktop input and
// presentation are mocked: tests never type into the user's foreground app.
#include "../Sources/Omnikey/win32/Omnikey/Omnikey/stdafx.h"
#include "../Sources/Omnikey/win32/Omnikey/Omnikey/AppDelegate.h"
#include "../Sources/Omnikey/win32/Omnikey/Omnikey/CandidatePanel.h"
#include <shlobj.h>
#include <iostream>
#include <cstdlib>

int vChineseMode = 1;
int vLanguage = 0;
static HWND foreground = reinterpret_cast<HWND>(100);
static std::wstring fakeExecutable, testData, committed;
static bool visible = false;
static std::vector<WORD> forwarded;

static void expect(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}

static HWND WINAPI testForeground() { return foreground; }
static UINT WINAPI testSendInput(UINT count, LPINPUT inputs, int size) {
    expect(size == sizeof(INPUT), "valid SendInput structure size");
    for (UINT i = 0; i < count; ++i) {
        expect(inputs[i].ki.dwExtraInfo == 1, "commit events carry Omnikey marker");
        if (!(inputs[i].ki.dwFlags & KEYEVENTF_KEYUP)) {
            if (inputs[i].ki.dwFlags & KEYEVENTF_UNICODE) committed += inputs[i].ki.wScan;
            else forwarded.push_back(inputs[i].ki.wVk);
        }
    }
    return count;
}
static BOOL WINAPI testShowWindow(HWND, int command) { visible = command != SW_HIDE; return TRUE; }
static BOOL WINAPI testPosition(HWND, HWND, int, int, int, int, UINT flags) {
    expect((flags & SWP_NOACTIVATE) != 0, "candidate panel never steals focus");
    if (flags & SWP_SHOWWINDOW) visible = true;
    return TRUE;
}
static BOOL WINAPI testLayered(HWND, HDC, POINT*, SIZE* size, HDC, POINT*, COLORREF, BLENDFUNCTION* blend, DWORD flags) {
    expect(flags == ULW_ALPHA && blend->AlphaFormat == AC_SRC_ALPHA, "popup uses per-pixel alpha");
    expect(size->cx > 100 && size->cy > 40, "popup has valid measured dimensions");
    return TRUE;
}
static DWORD WINAPI testModulePath(HMODULE, LPWSTR buffer, DWORD size) {
    wcscpy_s(buffer, size, fakeExecutable.c_str());
    return (DWORD)fakeExecutable.size();
}
static HRESULT WINAPI testDataPath(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR* output) {
    *output = (PWSTR)CoTaskMemAlloc((testData.size() + 1) * sizeof(wchar_t));
    wcscpy_s(*output, testData.size() + 1, testData.c_str());
    return S_OK;
}
static SHORT WINAPI testKeyState(int) { return 0; }
static int WINAPI testUnicode(UINT vk, UINT, const BYTE* state, LPWSTR output, int size, UINT flags, HKL) {
    expect(size >= 1 && flags == 4, "layout translation preserves dead-key state");
    if (vk >= 'A' && vk <= 'Z') {
        output[0] = (wchar_t)(vk + ((state[VK_SHIFT] & 0x80) ? 0 : 'a' - 'A'));
        return 1;
    }
    if (vk >= '0' && vk <= '9') { output[0] = (wchar_t)vk; return 1; }
    return 0;
}
static int WINAPI testMessageBox(HWND, LPCWSTR, LPCWSTR, UINT) {
    expect(false, "no unexpected input error dialog"); return IDOK;
}

AppDelegate::AppDelegate() {}
AppDelegate* AppDelegate::getInstance() { static AppDelegate instance; return &instance; }
void AppDelegate::selectInputMode(int mode) {
    ChineseInput::reset();
    vChineseMode = mode == 2;
    vLanguage = mode == 1;
}

#define GetForegroundWindow testForeground
#define SendInput testSendInput
#define ShowWindow testShowWindow
#define SetWindowPos testPosition
#define UpdateLayeredWindow testLayered
#define GetModuleFileNameW testModulePath
#define SHGetKnownFolderPath testDataPath
#define GetKeyState testKeyState
#define ToUnicodeEx testUnicode
#define MessageBoxW testMessageBox
#include "../Sources/Omnikey/win32/Omnikey/Omnikey/ChineseInput.cpp"

static void pump() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message); DispatchMessageW(&message);
    }
}
static bool key(unsigned vk, WPARAM event = WM_KEYDOWN, unsigned modifiers = 0) {
    KBDLLHOOKSTRUCT data = {}; data.vkCode = vk;
    return ChineseInput::handleKey(event, data, modifiers);
}
static void type(const char* text) {
    for (; *text; ++text) {
        unsigned vk = *text - 'a' + 'A';
        expect(key(vk), "Pinyin letter swallowed");
        expect(key(vk, WM_KEYUP), "paired letter release swallowed");
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) return 2;
    fakeExecutable = std::wstring(argv[1]) + L"\\Omnikey.exe";
    testData = argv[2];
    ChineseInput::start();
    DWORD deadline = GetTickCount() + 30000;
    while (!ChineseInput::ready() && (int)(deadline - GetTickCount()) > 0) { pump(); Sleep(10); }
    expect(ChineseInput::ready(), "async engine startup completes");
    auto app = AppDelegate::getInstance();
    app->cycleInputMode(); expect(vLanguage == 1 && vChineseMode == 0, "Chinese cycles to Vietnamese");
    app->cycleInputMode(); expect(vLanguage == 0 && vChineseMode == 0, "Vietnamese cycles to English");
    app->cycleInputMode(); expect(vLanguage == 0 && vChineseMode == 1, "English cycles to Chinese");
    expect(!key(VK_SPACE), "Space passes through while idle");
    expect(!key('1'), "digits pass through while idle");
    type("nihao"); // Deliberately queue a whole word and Space before pumping.
    expect(key(VK_SPACE), "queued composition captures Space");
    expect(key(VK_SPACE, WM_KEYUP), "Space release swallowed");
    pump();
    expect(committed == L"\u4f60\u597d", "queued letters/Space inject exactly 你好");
    expect(!visible, "candidate window hides after commit");
    committed.clear();
    type("zhongwen"); pump();
    expect(visible, "candidate panel appears during composition");
    auto composition = ChineseInput::current;
    auto layout = CandidatePanel::measure(composition, 144, 1200);
    expect(layout.height < 150, "normal popup is a compact horizontal strip");
    for (size_t i = 0; i < layout.cells.size(); ++i) {
        RECT cell = layout.cells[i];
        expect(CandidatePanel::hitTest(layout, { (cell.left + cell.right) / 2, (cell.top + cell.bottom) / 2 }) == (int)i, "candidate click targets match painted cells");
    }
    auto narrow = CandidatePanel::measure(composition, 192, 380);
    expect(narrow.width <= 380, "high-DPI popup wraps within screen width");
    expect(CandidatePanel::savePreview(composition, layout, false, (testData + L"\\candidate-light.png").c_str()), "light popup renders");
    expect(CandidatePanel::savePreview(composition, layout, true, (testData + L"\\candidate-dark.png").c_str()), "dark popup renders");
    expect(CandidatePanel::savePreview(composition, narrow, false, (testData + L"\\candidate-narrow.png").c_str()), "narrow popup renders");
    expect(key('1') && key('1', WM_KEYUP), "number selection captured"); pump();
    expect(committed == L"\u4e2d\u6587", "number selection injects 中文");
    committed.clear();
    type("ni"); pump();
    expect(key(VK_ESCAPE), "Escape captured while composing");
    key(VK_ESCAPE, WM_KEYUP); pump();
    expect(committed.empty() && !visible, "Escape hides panel without injecting text");
    type("nihao");
    ChineseInput::reset(); pump();
    expect(committed.empty() && !visible, "reset cancels queued input");
    type("nihao"); pump();
    expect(!key('C', WM_KEYDOWN, 0x02), "Ctrl+C passes through");
    expect(!visible && committed.empty(), "application shortcuts cancel composition");
    expect(!key('Z', WM_KEYDOWN, 0x04), "Alt hotkey passes through");
    expect(!key('R', WM_KEYDOWN, 0x20), "Win shortcut passes through");
    expect(key('N'), "Chinese letter starts composition");
    vChineseMode = 0;
    ChineseInput::reset();
    expect(key('N', WM_KEYUP), "release remains paired after changing mode");
    pump();
    expect(!key('A'), "English/Vietnamese mode passes through");
    vChineseMode = 1;
    type("nihao"); expect(key(VK_SPACE), "focus test queued commit");
    foreground = reinterpret_cast<HWND>(101);
    pump();
    expect(committed.empty() && !visible, "pending commit never reaches a different app");
    // A navigation key queued behind a commit must be forwarded if Rime declines it.
    type("nihao"); expect(key(VK_SPACE), "Space queues a commit");
    expect(key(VK_TAB), "Tab is queued behind pending composition"); pump();
    expect(!forwarded.empty() && forwarded.back() == VK_TAB, "unhandled Tab reaches the application");
    ChineseInput::stop();
    std::cout << "Windows Chinese input integration tests passed\n";
    return 0;
}
