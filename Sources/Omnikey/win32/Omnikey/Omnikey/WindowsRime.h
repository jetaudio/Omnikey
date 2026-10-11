#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "../../../macOS/Rime/rime_api.h"

struct ChineseComposition {
    std::wstring preedit;
    std::vector<std::wstring> candidates;
    std::vector<std::wstring> comments;
    std::wstring selectLabels;
    int highlighted = 0;
    int page = 0;
    bool lastPage = true;
    bool composing() const { return !preedit.empty() || !candidates.empty(); }
};

// A session is accessed from one thread at a time. Startup can run off the UI thread.
class WindowsRime {
public:
    ~WindowsRime();
    bool initialize(const std::wstring& bundle, const std::wstring& user);
    bool process(int keysym, int mask = 0);
    bool select(size_t index);
    void clear();
    std::wstring takeCommit();
    ChineseComposition composition();
    const std::wstring& error() const { return error_; }
private:
    HMODULE library_ = nullptr;
    RimeApi* api_ = nullptr;
    RimeSessionId session_ = 0;
    bool initialized_ = false;
    std::wstring error_;
    std::string shared_, user_, prebuilt_, staging_;
    WindowsRime(const WindowsRime&) = delete;
    WindowsRime& operator=(const WindowsRime&) = delete;
public:
    WindowsRime() = default;
};

namespace ChineseInput {
    void start();
    void stop();
    bool ready();
    std::wstring status();
    void reset();
    // Called by the low-level hook. Rime processing and painting are queued.
    bool handleKey(WPARAM message, const KBDLLHOOKSTRUCT& key, unsigned modifiers);
    bool containsWindow(HWND window);
}
