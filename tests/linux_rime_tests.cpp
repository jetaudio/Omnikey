// Chinese input on Linux: LinuxRime against Ubuntu's rime-data packages
// (rime-data-pinyin-simp, rime-prelude, rime-essay), as the IBus engine uses it.
#include "LinuxRime.h"

#include <ibus.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>

static int assertions = 0;

static void expect(bool condition, const std::string& label) {
    ++assertions;
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}

static std::string typeKeys(LinuxRime& rime, const std::string& keys, int finalKey) {
    for (char key : keys) rime.process(key, 0);
    if (finalKey) rime.process(finalKey, 0);
    return rime.takeCommit();
}

static int indexOf(const ChineseComposition& composition, const std::string& candidate) {
    auto it = std::find(composition.candidates.begin(), composition.candidates.end(), candidate);
    return it == composition.candidates.end() ? -1 : (int)(it - composition.candidates.begin());
}

// Pages forward until the character is on screen, then selects it.
static bool pickByPaging(LinuxRime& rime, const std::string& character) {
    for (int page = 0; page < 30; ++page) {
        int index = indexOf(rime.composition(), character);
        if (index >= 0) return rime.select((size_t)index);
        rime.process(IBUS_KEY_Page_Down, 0);
    }
    return false;
}

static bool key(unsigned keyval, unsigned state, int expectedKeysym, int expectedMask, bool composing = false) {
    int keysym = 0, mask = -1;
    return LinuxRime::keyFor(keyval, state, composing, keysym, mask) && keysym == expectedKeysym && mask == expectedMask;
}

static bool passes(unsigned keyval, unsigned state, bool composing = false) {
    int keysym = 0, mask = 0;
    return !LinuxRime::keyFor(keyval, state, composing, keysym, mask);
}

int main() {
    // IBus key presses -> Rime keys.
    expect(key(IBUS_KEY_a, 0, 'a', 0), "letter keysym");
    expect(key(IBUS_KEY_A, IBUS_SHIFT_MASK, 'A', 0), "shifted letter keeps its keysym without Shift");
    expect(key(IBUS_KEY_BackSpace, 0, IBUS_KEY_BackSpace, 0), "backspace keysym");
    expect(key(IBUS_KEY_space, 0, IBUS_KEY_space, 0), "space keysym");
    expect(key(IBUS_KEY_KP_Enter, 0, IBUS_KEY_Return, 0), "keypad enter is Return");
    expect(key(IBUS_KEY_Delete, IBUS_SHIFT_MASK, IBUS_KEY_Delete, 1), "shift+delete keeps shift");
    expect(key(IBUS_KEY_comma, 0, ',', 0), "punctuation keysym");
    expect(passes(IBUS_KEY_a, IBUS_CONTROL_MASK), "control letter belongs to the app when idle");
    expect(key(IBUS_KEY_k, IBUS_CONTROL_MASK, 'k', 1 << 2, true), "control letter reaches Rime while composing");
    expect(passes(IBUS_KEY_c, IBUS_SUPER_MASK, true), "super shortcuts always belong to the app");
    expect(passes(IBUS_KEY_A, IBUS_LOCK_MASK), "caps lock types Latin when idle");
    expect(key(IBUS_KEY_a, IBUS_LOCK_MASK | IBUS_MOD2_MASK, 'a', 0, true), "caps lock and num lock are ignored while composing");
    expect(passes(IBUS_KEY_F5, 0), "function keys pass through");

    const char* shared = std::getenv("OMNIKEY_RIME_SHARED");
    gchar* user = g_dir_make_tmp("omnikey-rime-XXXXXX", NULL);
    expect(user != NULL, "temporary user directory");
    {
        LinuxRime rime;
        expect(rime.initialize(shared ? shared : LinuxRime::kSharedDataDir, user), "Rime starts: " + rime.error());
        expect(!rime.composing(), "starts idle");

        expect(typeKeys(rime, "nihao", ' ') == "你好", "nihao + space");
        expect(typeKeys(rime, "jintiantianqihenhao", ' ') == "今天天气很好", "sentence");
        expect(typeKeys(rime, "nh", ' ') == "你好", "abbreviation");

        typeKeys(rime, "xian", 0);
        expect(rime.composing(), "composing while typing");
        ChineseComposition composition = rime.composition();
        expect(!composition.preedit.empty() && composition.cursor > 0, "preedit with cursor");
        int xian = indexOf(composition, "西安");
        expect(xian >= 0 && composition.candidates.size() == 7, "candidates page");
        expect(composition.comments.size() == composition.candidates.size(), "a comment slot per candidate");
        expect(rime.select((size_t)xian), "select by index");
        expect(rime.takeCommit() == "西安", "selected candidate commits");
        expect(!rime.composition().composing(), "composition ends after commit");

        expect(typeKeys(rime, "nihao", IBUS_KEY_Return) == "nihao", "return commits Latin letters");
        rime.process('n', 0);
        expect(rime.process(IBUS_KEY_Escape, 0) && !rime.composition().composing() && rime.takeCommit().empty(), "escape cancels");
        expect(typeKeys(rime, ",", 0) == "，", "full-width comma");
        expect(!rime.process(IBUS_KEY_Left, 0), "arrows pass through when idle");
        rime.process('w', 0);
        rime.clear();
        expect(!rime.composition().composing(), "clear composition");

        // Learning: a phrase built from single characters ranks first next time,
        // and Shift+Delete or Control+K forgets it again.
        typeKeys(rime, "laoshiren", 0);
        expect(rime.composition().candidates.empty() || rime.composition().candidates.front() != "捞尸人", "unlearned phrase is not first");
        for (const char* character : {"捞", "尸", "人"}) expect(pickByPaging(rime, character), std::string("pick ") + character);
        expect(rime.takeCommit() == "捞尸人", "phrase built from characters commits");
        for (int forget : {(int)IBUS_KEY_Delete, (int)'k'}) {
            typeKeys(rime, "laoshiren", 0);
            ChineseComposition learned = rime.composition();
            expect(!learned.candidates.empty() && learned.candidates.front() == "捞尸人", "learned phrase ranks first");
            expect(rime.process(forget, forget == 'k' ? (1 << 2) : 1), "forget key handled");
            expect(indexOf(rime.composition(), "捞尸人") < 0, "forgotten phrase leaves the page");
            rime.clear();
            if (forget == IBUS_KEY_Delete) { // learn again for the Control+K case
                typeKeys(rime, "laoshiren", 0);
                for (const char* character : {"捞", "尸", "人"}) pickByPaging(rime, character);
                rime.takeCommit();
            }
        }
    }
    gchar* command = g_strdup_printf("rm -rf '%s'", user);
    expect(system(command) == 0, "remove temporary user directory");
    g_free(command);
    g_free(user);
    std::cout << "Linux Rime tests: " << assertions << " assertions passed." << std::endl;
    return 0;
}
