#include "Engine.h"
#include <iostream>
#include <cstdlib>

#include "test_config.h"

static int assertions = 0;
static void expect(bool condition, const std::string& label) {
    ++assertions;
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}

static void resetConversion() {
    convertToolToAllCaps = convertToolToAllNonCaps = false;
    convertToolToCapsFirstLetter = convertToolToCapsEachWord = convertToolRemoveMark = false;
    convertToolFromCode = convertToolToCode = 0;
}

static std::string convert(const std::string& text, int from, int to) {
    convertToolFromCode = (Uint8)from;
    convertToolToCode = (Uint8)to;
    return convertUtil(text);
}

// Types lowercase Telex like the platform hooks do: backspaces, then the new
// characters (stored last first), then the typed key itself on a restore.
static std::string typeTelex(vKeyHookState* data, const std::string& keys) {
    static const std::pair<char, Uint16> letters[] = {
        {'a',KEY_A},{'b',KEY_B},{'c',KEY_C},{'d',KEY_D},{'e',KEY_E},{'f',KEY_F},{'g',KEY_G},{'h',KEY_H},{'i',KEY_I},
        {'j',KEY_J},{'k',KEY_K},{'l',KEY_L},{'m',KEY_M},{'n',KEY_N},{'o',KEY_O},{'p',KEY_P},{'q',KEY_Q},{'r',KEY_R},
        {'s',KEY_S},{'t',KEY_T},{'u',KEY_U},{'v',KEY_V},{'w',KEY_W},{'x',KEY_X},{'y',KEY_Y},{'z',KEY_Z}};
    auto letterFor = [&](Uint16 code) { for (auto& l : letters) if (l.second == code) return (wchar_t)l.first; return L'?'; };
    std::wstring text;
    startNewSession();
    for (char key : keys) {
        Uint16 code = 0;
        for (auto& l : letters) if (l.first == key) code = l.second;
        vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown, code, 0, false);
        if (data->code != vWillProcess && data->code != vRestore && data->code != vRestoreAndStartNewSession) {
            text += (wchar_t)key;
            continue;
        }
        for (int i = 0; i < data->backspaceCount && !text.empty(); i++) text.pop_back();
        for (int i = data->newCharCount - 1; i >= 0; i--) {
            Uint32 c = data->charData[i];
            text += (c & CHAR_CODE_MASK) ? (wchar_t)(c & CHAR_MASK) : letterFor((Uint16)c);
        }
        if (data->code != vWillProcess) text += (wchar_t)key;
    }
    return wideStringToUtf8(text);
}

int main() {
    resetConversion();
    expect(convertUtil(u8"Đặng THỊ Ánh Mixed ASCII") == u8"Đặng THỊ Ánh Mixed ASCII", "preserve original case");
    convertToolRemoveMark = true;
    expect(convertUtil(u8"ĐẶNG Thị Ánh Ước MƠ") == "DANG Thi Anh Uoc MO", "remove marks while preserving case");
    convertToolToAllCaps = true;
    expect(convertUtil(u8"Đặng Thị ánh") == "DANG THI ANH", "remove marks and uppercase");
    convertToolToAllCaps = false;
    convertToolToAllNonCaps = true;
    expect(convertUtil(u8"ĐẶNG Thị Ánh") == "dang thi anh", "remove marks and lowercase");
    convertToolToAllNonCaps = false;
    convertToolToCapsEachWord = true;
    expect(convertUtil(u8"đẶNG tHỊ áNH") == "Dang Thi Anh", "remove marks and capitalize words");
    convertToolToCapsEachWord = false;
    convertToolToCapsFirstLetter = true;
    expect(convertUtil(u8"đẶNG tHỊ. áNH\nước MƠ") == "Dang thi. Anh\nUoc mo", "capitalize sentences");
    resetConversion();

    // Every mapped Vietnamese character, all 25 source/destination pairs.
    for (const auto& row : _codeTable[0]) {
        for (size_t column = 0; column < row.second.size(); ++column) {
            Uint16 character = row.second[column];
            if (character == 0) continue; // Unused table slots.
            std::string original = wideStringToUtf8(std::wstring(1, character));
            for (int from = 0; from < 5; ++from) {
                std::string encoded = convert(original, 0, from);
                for (int to = 0; to < 5; ++to) {
                    std::string converted = convert(encoded, from, to);
                    std::string decoded = convert(converted, to, 0);
                    std::string expected = original;
                    if ((from == 1 || to == 1) && column % 2 == 0 &&
                        _codeTable[1][row.first][column] == _codeTable[1][row.first][column + 1]) {
                        expected = wideStringToUtf8(std::wstring(1, row.second[column + 1]));
                    }
                    expect(decoded == expected, "roundtrip U+" + std::to_string(character) +
                        " from " + std::to_string(from) + " to " + std::to_string(to));
                }
            }
        }
    }
    const std::string mixed = u8"Đặng THỊ Ánh, Ước MƠ! Ê Ắ Ấ Ỗ Ự Ỳ. Mixed ASCII 123 🚀";
    for (int code = 0; code < 5; ++code) {
        std::string encoded = convert(mixed, 0, code);
        std::string expected = code == 1 ? u8"Đặng THị ánh, Ước MƠ! Ê ắ ấ ỗ ự ỳ. Mixed ASCII 123 🚀" : mixed;
        expect(convert(encoded, code, 0) == expected, "mixed text roundtrip " + std::to_string(code));
        convertToolRemoveMark = true;
        expected = code == 1 ? u8"Dang THi anh, Uoc MO! E a a o u y. Mixed ASCII 123 🚀" : u8"Dang THI Anh, Uoc MO! E A A O U Y. Mixed ASCII 123 🚀";
        expect(convert(encoded, code, 0) == expected,
            "mixed text remove marks " + std::to_string(code));
        convertToolRemoveMark = false;
    }
    expect(convert("Keep This", 255, 0) == "Keep This", "invalid source encoding");
    expect(convert("Keep This", 0, 255) == "Keep This", "invalid target encoding");

    initSmartSwitchKey(nullptr, 0);
    const int packed = 1 | (3 << 1);
#if !defined(_WIN32) && !defined(LINUX)
    expect(getAppInputMethodStatus("com.apple.Terminal", packed) == (3 << 1), "developer default preserves code table");
#else
    expect(getAppInputMethodStatus("com.apple.Terminal", packed) == -1, "mac defaults do not affect Windows");
#endif
    setAppInputMethodStatus("com.apple.Terminal", packed);
    expect(getAppInputMethodStatus("com.apple.Terminal", 0) == packed, "explicit app preference wins");
    std::vector<Byte> saved;
    getSmartSwitchKeySaveData(saved);
    initSmartSwitchKey(saved.data(), (int)saved.size());
    expect(getAppInputMethodStatus("com.apple.Terminal", 0) == packed, "saved preference roundtrip");
    initSmartSwitchKey(nullptr, 0);
    expect(getAppInputMethodStatus("ordinary.app", packed) == -1, "new app inherits current language");
    expect(getAppInputMethodStatus("ordinary.app", 0) == packed, "new app keeps packed state");
    initSmartSwitchKey(nullptr, 0);
    expect(getAppInputMethodStatus("ordinary.app", 0) == -1, "reinitialization clears cache");
    const Byte truncated[] = {1, 0, 255, 'x'};
    initSmartSwitchKey(truncated, sizeof(truncated));
    getSmartSwitchKeySaveData(saved);
    expect(saved.size() == 2, "truncated preference data is ignored safely");
    expect(getAppInputMethodStatus("", packed) == -1, "unknown app does not become preference");

    // Tone placement in old (hòa) and modern (hoà) orthography. ia/ua are
    // placed by rule 4 of handleModernMark: khuấy must not become khúây.
    vKeyHookState* typing = (vKeyHookState*)vKeyInit();
    const struct { const char *keys, *modern, *old; } words[] = {
        {"tieengs", u8"tiếng", u8"tiếng"}, {"nguwowif", u8"người", u8"người"}, {"chieeuf", u8"chiều", u8"chiều"},
        {"yeeus", u8"yếu", u8"yếu"}, {"khuaays", u8"khuấy", u8"khuấy"}, {"nguaayr", u8"nguẩy", u8"nguẩy"},
        {"tuaans", u8"tuấn", u8"tuấn"}, {"chuaanr", u8"chuẩn", u8"chuẩn"}, {"quas", u8"quá", u8"quá"},
        {"gieets", u8"giết", u8"giết"}, {"hoaf", u8"hoà", u8"hòa"}, {"thuys", u8"thuý", u8"thúy"},
        {"khoer", u8"khoẻ", u8"khỏe"}, {"muwaf", u8"mừa", u8"mừa"}, {"nguwas", u8"ngứa", u8"ngứa"},
        {"khuyeenr", u8"khuyển", u8"khuyển"}, {"khuyur", u8"khuỷu", u8"khuỷu"}, {"thuowr", u8"thuở", u8"thuở"},
        {"ddaays", u8"đấy", u8"đấy"}, {"giuwax", u8"giữa", u8"giữa"}, {"kias", u8"kía", u8"kía"},
        {"muaf", u8"mùa", u8"mùa"}, {"thuyeenf", u8"thuyền", u8"thuyền"}, {"quyeets", u8"quyết", u8"quyết"},
    };
    for (int modern = 0; modern < 2; ++modern) {
        vUseModernOrthography = modern;
        for (const auto& word : words)
            expect(typeTelex(typing, word.keys) == (modern ? word.modern : word.old),
                std::string("tone placement ") + word.keys + (modern ? " modern" : " old"));
    }
    vUseModernOrthography = 0;

    // The macro key follows what is on screen: standalone w (ư), a restored
    // double key (aaa -> aa), quick Telex (cc -> ch) and a moved tone.
    auto tap = [&](Uint16 code) { vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown, code, 0, false); };
    auto macroFires = [&](std::initializer_list<Uint16> keys) {
        vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0);
        for (Uint16 code : keys) tap(code);
        tap(KEY_SPACE);
        return typing->code == vReplaceMaro;
    };
    vUseMacro = 1;
    initMacroMap(nullptr, 0);
    addMacro(u8"tư", "1"); addMacro("aa", "2"); addMacro(u8"tá", "3"); addMacro("ch", "4"); addMacro(u8"hoàn", "5");
    expect(macroFires({KEY_T, KEY_W}), "macro key with standalone w");
    expect(macroFires({KEY_A, KEY_A, KEY_A}), "macro key after a restored double key");
    expect(macroFires({KEY_T, KEY_A, KEY_S}), "macro key with a tone");
    expect(macroFires({KEY_H, KEY_O, KEY_F, KEY_A, KEY_N}), "macro key after the tone moves");
    expect(!macroFires({KEY_T, KEY_A}), "other words do not fire a macro");
    vQuickTelex = 1;
    expect(macroFires({KEY_C, KEY_C}), "macro key with quick Telex");
    vQuickTelex = 0;

    // Sizes are stored in one and two bytes; larger macros are refused and
    // damaged saved data is not read past its end.
    expect(!addMacro(std::string(300, 'a'), "x") && !addMacro("x", std::string(70000, 'a')), "oversized macro refused");
    std::vector<Byte> savedMacros;
    getMacroSaveData(savedMacros);
    initMacroMap(savedMacros.data(), (int)savedMacros.size());
    expect(hasMacro("ch") && hasMacro(u8"hoàn"), "macros survive a save and load");
    for (size_t cut = 0; cut < savedMacros.size(); ++cut) {
        std::vector<Byte> damaged(savedMacros.begin(), savedMacros.begin() + cut); // exact size: ASan sees overreads
        initMacroMap(damaged.data(), (int)damaged.size());
    }
    initMacroMap(nullptr, 0);
    vUseMacro = 0;

    // Text that is not UTF-8 (an ANSI macro file, a damaged clipboard) is
    // refused or left unchanged instead of throwing.
    expect(convertUtil("Vi\xEA\xF2t") == "Vi\xEA\xF2t", "non-UTF-8 text is returned unchanged");
    expect(!addMacro("\xEA\xF2", "x") && !hasMacro("\xEA\xF2") && !addMacro("", "x"), "macro without a valid name refused");

    // Quick consonants add a character; a word that fills the buffer is left alone.
    vQuickStartConsonant = vQuickEndConsonant = 1;
    vCheckSpelling = 0; vSetCheckSpelling();
    for (int length = 2; length <= MAX_BUFF + 8; ++length) {
        vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0);
        tap(KEY_F);
        for (int n = 1; n < length - 2; ++n) tap(KEY_B);
        if (length > 2) tap(KEY_A);
        tap(KEY_G);
        tap(KEY_SPACE);
        expect(typing->newCharCount <= MAX_BUFF, "quick consonant output fits the buffer");
        if (length == 3) expect(typing->code == vRestore && typing->newCharCount == 5, "fag expands to phang");
    }
    vQuickStartConsonant = vQuickEndConsonant = 0;
    vCheckSpelling = 1; vSetCheckSpelling();

    // A code table index outside the tables falls back to Unicode.
    for (int table : {-1, 5, 99}) {
        vCodeTable = table;
        expect(typeTelex(typing, "tieengs") == u8"tiếng", "invalid code table falls back to Unicode");
    }
    vCodeTable = 0;
    std::cout << "Engine regression tests: " << assertions << " assertions passed.\n";
}
