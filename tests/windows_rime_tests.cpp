#include "../Sources/Omnikey/win32/Omnikey/Omnikey/WindowsRime.h"
#include <iostream>
#include <algorithm>
#include <cstdlib>

static void expect(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}

static void type(WindowsRime& rime, const char* text) {
    for (; *text; ++text) expect(rime.process(*text), "Pinyin letter consumed");
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) { std::cerr << "Usage: windows-rime-tests <Rime bundle> <test user dir>\n"; return 2; }
    {
        WindowsRime missing;
        expect(!missing.initialize(std::wstring(argv[1]) + L"\\missing", argv[2]), "missing bundle fails cleanly");
        expect(!missing.error().empty(), "missing bundle has an actionable error");
        expect(!missing.process('n'), "unavailable Rime passes through");
        expect(!missing.composition().composing(), "unavailable Rime has no composition");
    }
    WindowsRime rime;
    expect(rime.initialize(argv[1], argv[2]), "Rime initializes from packaged data");
    expect(rime.takeCommit().empty(), "no spurious startup commit");
    type(rime, "nihao");
    auto composition = rime.composition();
    auto candidate = std::find(composition.candidates.begin(), composition.candidates.end(), L"\u4f60\u597d");
    expect(candidate != composition.candidates.end(), "nihao offers 你好");
    expect(rime.select(candidate - composition.candidates.begin()), "current-page candidate selection");
    expect(rime.takeCommit() == L"\u4f60\u597d", "candidate commits Unicode 你好");
    expect(rime.takeCommit().empty(), "commit is drained once");
    expect(!rime.composition().composing(), "candidate commit clears composition");
    type(rime, "zhongwen");
    expect(rime.process(' '), "Space selects candidate");
    expect(rime.takeCommit() == L"\u4e2d\u6587", "Space commits 中文");
    type(rime, "nihao");
    expect(rime.process('1'), "number selects candidate");
    expect(rime.takeCommit() == L"\u4f60\u597d", "number commits 你好");
    type(rime, "ni");
    expect(rime.process(0xff08), "Backspace edits Pinyin");
    expect(rime.composition().preedit == L"n", "Backspace removes the last letter");
    expect(rime.process(0xff1b), "Escape cancels Pinyin");
    expect(!rime.composition().composing() && rime.takeCommit().empty(), "Escape cancels without committing");
    type(rime, "shi");
    rime.process(0xff56);
    expect(rime.composition().page > 0, "PageDown advances candidates");
    rime.process(0xff55);
    expect(rime.composition().page == 0, "PageUp returns to first page");
    rime.clear();
    type(rime, "nihao");
    rime.clear();
    expect(!rime.composition().composing(), "focus/mode reset clears composition");
    expect(rime.takeCommit().empty(), "reset does not leak a commit into another app");
    std::cout << "Windows Rime regression tests passed\n";
    return 0;
}
