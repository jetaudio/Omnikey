//
//  OmnikeyEngine.cpp
//  Omnikey - Linux IBus engine
//
//  Feeds IBus key events to the shared Omnikey engine and applies its result
//  (backspace count + new characters) in one of two ways:
//   - Preedit: the current word is kept as preedit text and committed at a word
//     break. It never has to delete text the application owns, so it works in
//     every app, terminals included.
//   - Direct: text is committed as you type and corrected with
//     delete_surrounding_text (or forwarded BackSpace keys when the app does not
//     support surrounding text), like Omnikey does on macOS and Windows.
//  In 中 mode keys go to Rime instead, and IBus shows its preedit and candidates.
//

#include "OmnikeyEngine.h"
#include "Config.h"
#include "Engine.h"
#include "LinuxRime.h"
#include "VirtualKeyboard.h"

#include <string>
#include <vector>
#include <algorithm>

#ifndef OMNIKEY_SETUP_PATH
#define OMNIKEY_SETUP_PATH "omnikey-setup"
#endif

#define EVDEV_BACKSPACE 14
#define RECENT_TEXT_LIMIT 64
#define PENDING_TIMEOUT_MS 300
#define COMMIT_DELAY_MS 15
#define CURSOR_CHECK_MS 150

struct QueuedKey {
    guint keyval, keycode, state;
    bool resendOnly; // already handled; the app still has to receive it
};

// What happens to a key press after Omnikey has seen it.
enum KeyResult {
    KeyPass = 0,          // the app receives the key
    KeyConsumed = 1,
    KeyPassAfterPending   // the app receives it once the pending text is committed
};
#define MODIFIER_MASK (IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK | IBUS_MOD4_MASK | IBUS_HYPER_MASK | IBUS_META_MASK)

enum InputMode { ModeVietnamese = 0, ModeEnglish, ModeChinese, ModeCount };
static const char* const MODE_KEYS[ModeCount] = {"InputMode.Vietnamese", "InputMode.English", "InputMode.Chinese"};
static const char* const MODE_LABELS[ModeCount] = {"Tiếng Việt", "English", "中文 (Pinyin)"};
static const char* const MODE_SYMBOLS[ModeCount] = {"V", "E", "中"};

struct OmnikeyEngine {
    IBusEngine parent;
    IBusPropList* props;
    IBusProperty* modeProp;            // menu; GNOME shows its symbol in the top bar
    IBusProperty* modeItems[ModeCount]; // owned by modeProp
    // Preedit mode: the current word, one entry per engine character
    // (an entry holds two code points for compound Unicode and VNI Windows).
    std::vector<std::u32string>* word;
    // Direct mode: code points used by each recently sent engine character.
    std::vector<int>* sentLengths;
    // Direct mode: what we believe is just before the cursor, used to check
    // that delete-surrounding-text would remove exactly our own text.
    std::u32string* recent;
    // Direct mode with the virtual keyboard: text waiting until the app has
    // received our BackSpaces, and keys typed meanwhile.
    std::u32string* pendingText;
    int pendingPresses, pendingReleases;
    guint pendingTimer;
    std::vector<QueuedKey>* queue;
    // Keys re-sent through the virtual keyboard that must pass straight through.
    std::vector<guint>* passPresses;
    std::vector<guint>* passReleases;
    // While replaying queued keys, text is collected and committed once:
    // Wayland's text-input protocol keeps only the last of several commits
    // sent together, which dropped characters when typing fast.
    std::u32string* batch;
    bool batching;
    // Detects a click elsewhere: the cursor no longer follows what we typed.
    guint cursorCheckTimer;
    // Modifier-only tap tracking (Ctrl+Shift switch, Ctrl/Alt temporary off).
    guint tapMask;
    gboolean tapValid;
};

struct OmnikeyEngineClass {
    IBusEngineClass parent;
};

G_DEFINE_TYPE(OmnikeyEngine, omnikey_engine, IBUS_TYPE_ENGINE)

static vKeyHookState* pData = NULL;
static GList* engines = NULL;
static GFileMonitor* configMonitor = NULL;
static int loadedCodeTable = -1;
// Chinese input: one Rime session for all input contexts, started in the background.
static LinuxRime* rime = NULL;
static bool rimeStarting = false;
static std::string rimeError;

/*------------------------------------------------------------------------------*/
// Key helpers

static bool isLetterKey(Uint16 key) {
    static const Uint16 letters[] = {
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
        KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
    return std::find(std::begin(letters), std::end(letters), key) != std::end(letters);
}

static bool isDigitKey(Uint16 key) {
    return IS_NUMBER_KEY(key);
}

static bool isPunctuationKey(Uint16 key) {
    static const Uint16 keys[] = {
        KEY_LEFT_BRACKET, KEY_RIGHT_BRACKET, KEY_BACKQUOTE, KEY_MINUS, KEY_EQUALS, KEY_BACK_SLASH,
        KEY_SEMICOLON, KEY_QUOTE, KEY_COMMA, KEY_DOT, KEY_SLASH};
    return std::find(std::begin(keys), std::end(keys), key) != std::end(keys);
}

static bool isControlKey(Uint16 key) {
    return key == KEY_ESC || key == KEY_TAB || key == KEY_ENTER || key == KEY_LEFT ||
           key == KEY_RIGHT || key == KEY_UP || key == KEY_DOWN;
}

// Keys the engine understands; anything else ends the word and passes through.
static bool isEngineKey(Uint16 key) {
    return isLetterKey(key) || isDigitKey(key) || isPunctuationKey(key) || isControlKey(key) ||
           key == KEY_SPACE || key == KEY_DELETE;
}

static guint modifierBit(guint keyval) {
    switch (keyval) {
        case IBUS_KEY_Shift_L: case IBUS_KEY_Shift_R: return IBUS_SHIFT_MASK;
        case IBUS_KEY_Control_L: case IBUS_KEY_Control_R: return IBUS_CONTROL_MASK;
        case IBUS_KEY_Alt_L: case IBUS_KEY_Alt_R: case IBUS_KEY_Meta_L: case IBUS_KEY_Meta_R: return IBUS_MOD1_MASK;
        case IBUS_KEY_Super_L: case IBUS_KEY_Super_R: case IBUS_KEY_Hyper_L: case IBUS_KEY_Hyper_R: return IBUS_SUPER_MASK;
        case IBUS_KEY_ISO_Level3_Shift: case IBUS_KEY_Caps_Lock: case IBUS_KEY_Num_Lock: return IBUS_LOCK_MASK;
    }
    return 0;
}

// Converts one engine character to the code points of the selected code table.
static std::u32string encodeChar(Uint32 data) {
    std::u32string out;
    if (data & PURE_CHARACTER_MASK) {
        out += (char32_t)(data & CHAR_MASK);
    } else if (!(data & CHAR_CODE_MASK)) {
        Uint16 ch = keyCodeToCharacter(data);
        if (ch) out += (char32_t)ch;
    } else {
        Uint16 ch = (Uint16)data;
        if (vCodeTable == 0) {
            out += (char32_t)ch;
        } else if (vCodeTable == 1 || vCodeTable == 2 || vCodeTable == 4) { // 1 byte code tables
            out += (char32_t)LOBYTE(ch);
            if (HIBYTE(ch) > 32) out += (char32_t)HIBYTE(ch);
        } else if (vCodeTable == 3) { // Unicode compound
            Uint16 mark = ch >> 13;
            out += (char32_t)(ch & 0x1FFF);
            if (mark > 0) out += (char32_t)_unicodeCompoundMark[mark - 1];
        }
    }
    return out;
}

// The engine's new characters in typing order, followed by the macro content.
static std::vector<std::u32string> engineOutput() {
    std::vector<std::u32string> chars;
    if (pData->code == vReplaceMaro) {
        for (Uint32 data : pData->macroData) chars.push_back(encodeChar(data));
    } else {
        for (int i = pData->newCharCount - 1; i >= 0; i--) chars.push_back(encodeChar(pData->charData[i]));
    }
    return chars;
}

static IBusText* textFrom(const std::u32string& str) {
    gchar* utf8 = g_ucs4_to_utf8((const gunichar*)str.data(), (glong)str.size(), NULL, NULL, NULL);
    IBusText* text = ibus_text_new_from_string(utf8 ? utf8 : "");
    g_free(utf8);
    return text;
}

// Clears every piece of engine state, including the saved previous words, so a
// later key can never ask to rewrite text that was already committed.
static void hardReset() {
    vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0);
    startNewSession();
}

/*------------------------------------------------------------------------------*/
// Output

static std::u32string joinedWord(OmnikeyEngine* self) {
    std::u32string all;
    for (const auto& ch : *self->word) all += ch;
    return all;
}

static void updatePreedit(OmnikeyEngine* self) {
    std::u32string all = joinedWord(self);
    if (all.empty()) {
        ibus_engine_hide_preedit_text((IBusEngine*)self);
        return;
    }
    IBusText* text = textFrom(all);
    ibus_text_append_attribute(text, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, (gint)all.size());
    ibus_engine_update_preedit_text_with_mode((IBusEngine*)self, text, (guint)all.size(), TRUE, IBUS_ENGINE_PREEDIT_COMMIT);
}

static void commitString(OmnikeyEngine* self, const std::u32string& str) {
    if (str.empty()) return;
    if (self->batching) *self->batch += str;
    else ibus_engine_commit_text((IBusEngine*)self, textFrom(str));
}

static void flushBatch(OmnikeyEngine* self) {
    if (self->batch->empty()) return;
    ibus_engine_commit_text((IBusEngine*)self, textFrom(*self->batch));
    self->batch->clear();
}

// Commits the preedit word, if any.
static void flushWord(OmnikeyEngine* self) {
    if (self->word->empty()) return;
    std::u32string all = joinedWord(self);
    self->word->clear();
    ibus_engine_hide_preedit_text((IBusEngine*)self);
    commitString(self, all);
}

static void rememberText(OmnikeyEngine* self, const std::u32string& text) {
    auto& recent = *self->recent;
    recent += text;
    if (recent.size() > RECENT_TEXT_LIMIT) recent.erase(0, recent.size() - RECENT_TEXT_LIMIT);
}

static void forgetText(OmnikeyEngine* self, int count) {
    auto& recent = *self->recent;
    if ((size_t)count >= recent.size()) recent.clear();
    else recent.resize(recent.size() - count);
}

// The text before the cursor as the app last reported it. False when the app
// does not report surrounding text (terminals).
static bool textBeforeCursor(OmnikeyEngine* self, std::u32string& before, bool& selection) {
    IBusEngine* engine = (IBusEngine*)self;
    if (!(engine->client_capabilities & IBUS_CAP_SURROUNDING_TEXT)) return false;
    IBusText* text = NULL;
    guint cursor = 0, anchor = 0;
    ibus_engine_get_surrounding_text(engine, &text, &cursor, &anchor);
    if (!text) return false;
    glong length = 0;
    gunichar* chars = g_utf8_to_ucs4_fast(ibus_text_get_text(text), -1, &length);
    guint start = MIN(cursor, anchor);
    before.assign((const char32_t*)chars, MIN((glong)start, length));
    g_free(chars);
    selection = cursor != anchor;
    return true;
}

static bool endsWith(const std::u32string& text, const std::u32string& tail) {
    return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

// True when the app reports surrounding text whose last `count` characters
// before the cursor are the ones we typed, with nothing selected.
static bool canDeleteSurrounding(OmnikeyEngine* self, int count) {
    const auto& recent = *self->recent;
    if ((size_t)count > recent.size()) return false;
    std::u32string before;
    bool selection = false;
    if (!textBeforeCursor(self, before, selection) || selection) return false;
    return endsWith(before, recent.substr(recent.size() - count));
}

// An autocompleted suggestion is usually selected after the cursor; the first
// BackSpace would remove it instead of a character.
static bool hasSelection(OmnikeyEngine* self) {
    std::u32string before;
    bool selection = false;
    return textBeforeCursor(self, before, selection) && selection;
}

static void processQueue(OmnikeyEngine* self);

static void finishPending(OmnikeyEngine* self) {
    if (self->pendingTimer) g_source_remove(self->pendingTimer);
    self->pendingTimer = 0;
    self->pendingPresses = self->pendingReleases = 0;
    std::u32string text = *self->pendingText;
    self->pendingText->clear();
    self->batching = true;
    commitString(self, text);
    processQueue(self);
    self->batching = false;
    flushBatch(self);
}

static gboolean onPendingTimer(gpointer data) {
    OmnikeyEngine* self = (OmnikeyEngine*)data;
    self->pendingTimer = 0;
    finishPending(self);
    return G_SOURCE_REMOVE;
}

static bool isPending(OmnikeyEngine* self) {
    return self->pendingTimer != 0;
}

// Replaces the `count` characters before the cursor with `text`.
static void replaceBefore(OmnikeyEngine* self, int count, const std::u32string& text) {
    IBusEngine* engine = (IBusEngine*)self;
    // While a batch is collected there must be no second commit, so only
    // BackSpaces are used then.
    bool surrounding = count > 0 && !self->batching && canDeleteSurrounding(self, count);
    forgetText(self, count);
    rememberText(self, text);
    if (count > 0 && !surrounding && VirtualKeyboard::isOpen() && hasSelection(self)) {
        // Replace the selection with a placeholder that the BackSpaces remove too,
        // like Omnikey's "fix browser suggestion" option on Windows. It goes out
        // with any batched text, so it is still a single commit.
        commitString(self, U"\u202F");
        count++;
    }
    // A deletion must apply after the text collected so far.
    if (count > 0 && !VirtualKeyboard::isOpen()) flushBatch(self);
    if (count <= 0 || surrounding) {
        if (surrounding) {
            flushBatch(self);
            ibus_engine_delete_surrounding_text(engine, -count, (guint)count);
        }
        commitString(self, text);
    } else if (VirtualKeyboard::isOpen()) {
        // Real BackSpaces reach every app. The text is committed once the last
        // one has come back through IBus, so it cannot overtake them.
        for (int i = 0; i < count; i++) VirtualKeyboard::tap(EVDEV_BACKSPACE);
        self->pendingPresses = self->pendingReleases = count;
        *self->pendingText = text;
        self->pendingTimer = g_timeout_add(PENDING_TIMEOUT_MS, onPendingTimer, self);
    } else {
        for (int i = 0; i < count; i++) {
            ibus_engine_forward_key_event(engine, IBUS_KEY_BackSpace, EVDEV_BACKSPACE, 0);
            ibus_engine_forward_key_event(engine, IBUS_KEY_BackSpace, EVDEV_BACKSPACE, IBUS_RELEASE_MASK);
        }
        commitString(self, text);
    }
}

static void cancelPending(OmnikeyEngine* self) {
    if (isPending(self)) finishPending(self);
    self->queue->clear();
}

/*------------------------------------------------------------------------------*/
// Chinese mode (中)

static void showComposition(OmnikeyEngine* self) {
    IBusEngine* engine = (IBusEngine*)self;
    ChineseComposition composition = rime->composition();
    if (composition.preedit.empty()) {
        ibus_engine_hide_preedit_text(engine);
    } else {
        IBusText* text = ibus_text_new_from_string(composition.preedit.c_str());
        ibus_text_append_attribute(text, IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, (gint)ibus_text_get_length(text));
        // Unlike a Vietnamese word, unfinished Pinyin is dropped when the app resets.
        ibus_engine_update_preedit_text(engine, text, (guint)composition.cursor, TRUE);
    }
    if (composition.candidates.empty()) {
        ibus_engine_hide_lookup_table(engine);
        return;
    }
    // Rime pages the candidates itself; the table holds the current page.
    IBusLookupTable* table = ibus_lookup_table_new((guint)composition.candidates.size(), 0, TRUE, FALSE);
    for (size_t i = 0; i < composition.candidates.size(); i++) {
        std::string candidate = composition.candidates[i];
        if (!composition.comments[i].empty()) candidate += " " + composition.comments[i];
        ibus_lookup_table_append_candidate(table, ibus_text_new_from_string(candidate.c_str()));
    }
    ibus_lookup_table_set_cursor_pos(table, (guint)MAX(composition.highlighted, 0));
    ibus_engine_update_lookup_table(engine, table, TRUE);
}

static void commitChinese(OmnikeyEngine* self) {
    std::string commit = rime->takeCommit();
    if (!commit.empty()) ibus_engine_commit_text((IBusEngine*)self, ibus_text_new_from_string(commit.c_str()));
}

static gboolean processChinese(OmnikeyEngine* self, guint keyval, guint state) {
    if (!rime) return FALSE; // still loading or unavailable: type Latin
    int keysym = 0, mask = 0;
    if (!LinuxRime::keyFor(keyval, state, rime->composing(), keysym, mask)) return FALSE;
    gboolean handled = rime->process(keysym, mask);
    commitChinese(self);
    showComposition(self);
    return handled;
}

// A key Rime handles the same way as the matching button in the candidate window.
static void processChineseKey(OmnikeyEngine* self, int keysym) {
    if (!vChineseMode || !rime) return;
    rime->process(keysym, 0);
    commitChinese(self);
    showComposition(self);
}

static void clearChinese(OmnikeyEngine* self) {
    if (rime) rime->clear();
    ibus_engine_hide_lookup_table((IBusEngine*)self);
}

static void updateAllModeProperties();

struct RimeStart {
    LinuxRime* rime;
    bool ok;
};

static gboolean onRimeStarted(gpointer data) {
    RimeStart* start = (RimeStart*)data;
    rimeStarting = false;
    if (start->ok) {
        rime = start->rime;
    } else {
        rimeError = start->rime->error();
        g_warning("Omnikey: Chinese input is unavailable: %s", rimeError.c_str());
        delete start->rime;
    }
    delete start;
    updateAllModeProperties();
    return G_SOURCE_REMOVE;
}

static gpointer startRime(gpointer) {
    RimeStart* start = new RimeStart{new LinuxRime(), false};
    gchar* user = g_build_filename(g_get_user_data_dir(), "omnikey", "rime", NULL);
    start->ok = start->rime->initialize(LinuxRime::kSharedDataDir, user);
    g_free(user);
    g_idle_add(onRimeStarted, start);
    return NULL;
}

// Rime deploys its data on first use, which can take a while.
static void startChinese() {
    if (rime || rimeStarting || !rimeError.empty()) return;
    rimeStarting = true;
    g_thread_unref(g_thread_new("omnikey-rime", startRime, NULL));
}

static void resetWord(OmnikeyEngine* self) {
    self->word->clear();
    self->sentLengths->clear();
    self->recent->clear();
    ibus_engine_hide_preedit_text((IBusEngine*)self);
    clearChinese(self);
    hardReset();
}

/*------------------------------------------------------------------------------*/
// Language switching and properties

static int currentMode() {
    return vChineseMode ? ModeChinese : (vLanguage ? ModeVietnamese : ModeEnglish);
}

static void updateModeProperty(OmnikeyEngine* self) {
    int mode = currentMode();
    ibus_property_set_label(self->modeProp, ibus_text_new_from_static_string(MODE_LABELS[mode]));
    ibus_property_set_symbol(self->modeProp, ibus_text_new_from_static_string(MODE_SYMBOLS[mode]));
    std::string tooltip = "Chuyển chế độ gõ";
    if (mode == ModeChinese && rimeStarting) tooltip = "Đang tải bộ gõ Pinyin…";
    else if (mode == ModeChinese && !rime) tooltip = "Chưa gõ được tiếng Trung: " + rimeError;
    ibus_property_set_tooltip(self->modeProp, ibus_text_new_from_string(tooltip.c_str()));
    for (int i = 0; i < ModeCount; i++) {
        ibus_property_set_state(self->modeItems[i], i == mode ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED);
        ibus_engine_update_property((IBusEngine*)self, self->modeItems[i]);
    }
    ibus_engine_update_property((IBusEngine*)self, self->modeProp);
}

static void updateAllModeProperties() {
    for (GList* it = engines; it; it = it->next) updateModeProperty((OmnikeyEngine*)it->data);
}

static void setMode(OmnikeyEngine* self, int mode) {
    if (mode != currentMode()) {
        flushWord(self);
        resetWord(self);
        vChineseMode = mode == ModeChinese ? 1 : 0;
        vLanguage = mode == ModeVietnamese ? 1 : 0;
        OmnikeyConfig::save();
    }
    if (vChineseMode) {
        if (!rime && !rimeStarting) rimeError.clear(); // try again, the data may be installed now
        startChinese();
    }
    updateAllModeProperties();
}

// The switch key cycles V -> E -> 中 -> V, as on macOS and Windows.
static void cycleMode(OmnikeyEngine* self) {
    int mode = currentMode();
    setMode(self, mode == ModeVietnamese ? ModeEnglish : (mode == ModeEnglish ? ModeChinese : ModeVietnamese));
}

static void onModifierTap(OmnikeyEngine* self, guint mask) {
    if (mask == (IBUS_CONTROL_MASK | IBUS_SHIFT_MASK) && vSwitchKey == SwitchCtrlShift) {
        cycleMode(self);
    } else if (mask == IBUS_CONTROL_MASK && vTempOffSpelling && vLanguage) {
        vTempOffSpellChecking();
    } else if (mask == IBUS_MOD1_MASK && vTempOffOmnikey && vLanguage) {
        vTempOffEngine();
    }
}

static gboolean isSwitchHotkey(guint keyval, guint state) {
    guint mods = state & (MODIFIER_MASK | IBUS_SHIFT_MASK);
    if (vSwitchKey == SwitchCtrlSpace)
        return keyval == IBUS_KEY_space && mods == IBUS_CONTROL_MASK;
    if (vSwitchKey == SwitchAltZ)
        return (keyval == IBUS_KEY_z || keyval == IBUS_KEY_Z) && mods == IBUS_MOD1_MASK;
    return FALSE;
}

/*------------------------------------------------------------------------------*/
// Key handling

static Uint8 capsStatus(guint state) {
    bool shift = state & IBUS_SHIFT_MASK, caps = state & IBUS_LOCK_MASK;
    return (shift && caps) ? 0 : (shift ? 1 : (caps ? 2 : 0));
}

static gboolean processPreedit(OmnikeyEngine* self, Uint16 key, guint keyval, guint state) {
    auto& word = *self->word;
    if (key == KEY_DELETE && word.empty()) return FALSE; // nothing of ours to delete

    vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown, key, capsStatus(state), false);

    gunichar typed = ibus_keyval_to_unicode(keyval);
    bool printable = typed >= 0x20 && !isControlKey(key) && key != KEY_DELETE;
    bool passKey = false;

    if (pData->code == vDoNothing) {
        if (key == KEY_DELETE) word.pop_back();
        else if (printable) word.push_back(std::u32string(1, typed));
        else passKey = true;
    } else {
        size_t remove = std::min<size_t>(pData->backspaceCount, word.size());
        word.resize(word.size() - remove);
        for (auto& ch : engineOutput()) word.push_back(ch);
        if (pData->code != vWillProcess) { // restore or macro: the typed key follows
            if (printable) word.push_back(std::u32string(1, typed));
            else passKey = true;
        }
        if (pData->code == vRestoreAndStartNewSession) startNewSession();
    }

    bool inWord = (isLetterKey(key) || key == KEY_LEFT_BRACKET || key == KEY_RIGHT_BRACKET ||
                   (isDigitKey(key) && !(state & IBUS_SHIFT_MASK)) || key == KEY_DELETE) && !passKey;
    if (!inWord) {
        flushWord(self);
    } else {
        updatePreedit(self);
        if (word.empty()) hardReset();
    }
    return passKey ? FALSE : TRUE;
}

static int processDirect(OmnikeyEngine* self, Uint16 key, guint keyval, guint state) {
    auto& sent = *self->sentLengths;
    vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown, key, capsStatus(state), false);

    gunichar typed = ibus_keyval_to_unicode(keyval);
    bool printable = typed >= 0x20 && !isControlKey(key) && key != KEY_DELETE;

    if (pData->code == vDoNothing) {
        if (key == KEY_DELETE) {
            int length = sent.empty() ? 1 : sent.back();
            if (!sent.empty()) sent.pop_back();
            if (length > 1) { // a compound character takes more than one backspace
                replaceBefore(self, length, U"");
                return KeyConsumed;
            }
            forgetText(self, 1);
        } else if (pData->extCode == 1 && !printable) {
            sent.clear();
            self->recent->clear();
        } else if (printable) {
            sent.push_back(1);
            rememberText(self, std::u32string(1, typed));
        }
        return KeyPass;
    }

    int units = 0;
    for (int i = 0; i < pData->backspaceCount; i++) {
        units += sent.empty() ? 1 : sent.back();
        if (!sent.empty()) sent.pop_back();
    }

    std::u32string out;
    for (auto& ch : engineOutput()) {
        out += ch;
        sent.push_back((int)ch.size());
    }
    bool passKey = false;
    if (pData->code != vWillProcess) {
        if (printable) {
            out += (char32_t)typed;
            sent.push_back(1);
        } else {
            passKey = true;
        }
    }
    replaceBefore(self, units, out);
    if (pData->code == vRestoreAndStartNewSession) {
        startNewSession();
        sent.clear();
    }
    // Enter, Tab, ... must follow the corrected word.
    if (passKey && isPending(self)) return KeyPassAfterPending;
    return passKey ? KeyPass : KeyConsumed;
}

// Handles a key press that is not a bare modifier.
static int handleKeyPress(OmnikeyEngine* self, guint keyval, guint keycode, guint state) {
    if (isSwitchHotkey(keyval, state)) {
        cycleMode(self);
        return KeyConsumed;
    }

    if (vChineseMode) return processChinese(self, keyval, state) ? KeyConsumed : KeyPass;
    if (!vLanguage) return KeyPass;

    Uint16 key = (Uint16)(keycode + 8); // evdev -> X11 key codes used by the engine
    if ((state & MODIFIER_MASK) || !isEngineKey(key)) {
        flushWord(self);
        resetWord(self);
        return KeyPass;
    }

    if (vOutputMode == OutputDirect) return processDirect(self, key, keyval, state);
    return processPreedit(self, key, keyval, state) ? KeyConsumed : KeyPass;
}

// Re-sends a key the app should get after text that is still pending.
static void resendKey(OmnikeyEngine* self, guint keyval, guint keycode, guint state) {
    gunichar ch = ibus_keyval_to_unicode(keyval);
    if (ch >= 0x20 && !(state & MODIFIER_MASK)) {
        std::u32string text(1, (char32_t)ch);
        rememberText(self, text);
        commitString(self, text);
    } else if (VirtualKeyboard::isOpen()) {
        self->passPresses->push_back(keycode);
        VirtualKeyboard::tap((int)keycode);
    }
}

static void processQueue(OmnikeyEngine* self) {
    while (!self->queue->empty() && !isPending(self)) {
        QueuedKey key = self->queue->front();
        self->queue->erase(self->queue->begin());
        int result = key.resendOnly ? KeyPass : handleKeyPress(self, key.keyval, key.keycode, key.state);
        if (result == KeyPass) resendKey(self, key.keyval, key.keycode, key.state);
        else if (result == KeyPassAfterPending)
            self->queue->insert(self->queue->begin(), {key.keyval, key.keycode, key.state, true});
    }
}

static gboolean takeKey(std::vector<guint>& keys, guint keycode) {
    auto it = std::find(keys.begin(), keys.end(), keycode);
    if (it == keys.end()) return FALSE;
    keys.erase(it);
    return TRUE;
}

static gboolean omnikey_engine_process_key_event(IBusEngine* engine, guint keyval, guint keycode, guint state) {
    OmnikeyEngine* self = (OmnikeyEngine*)engine;
    gboolean release = (state & IBUS_RELEASE_MASK) != 0;

    // Our own virtual keyboard events coming back through IBus.
    if (!release && takeKey(*self->passPresses, keycode)) {
        self->passReleases->push_back(keycode);
        return FALSE;
    }
    if (release && takeKey(*self->passReleases, keycode)) return FALSE;
    if (isPending(self) && keycode == EVDEV_BACKSPACE) {
        if (!release && self->pendingPresses > 0) {
            self->pendingPresses--;
            return FALSE;
        }
        if (release && self->pendingReleases > 0) {
            if (--self->pendingReleases == 0) {
                // Give the compositor a moment to hand the last BackSpace to the app.
                g_source_remove(self->pendingTimer);
                self->pendingTimer = g_timeout_add(COMMIT_DELAY_MS, onPendingTimer, self);
            }
            return FALSE;
        }
    }

    guint bit = modifierBit(keyval);
    if (release) {
        if (bit && bit != IBUS_LOCK_MASK) {
            guint remaining = state & (IBUS_SHIFT_MASK | IBUS_CONTROL_MASK | IBUS_MOD1_MASK | IBUS_SUPER_MASK) & ~bit;
            if (remaining == 0) {
                if (self->tapValid && self->tapMask) onModifierTap(self, self->tapMask);
                self->tapMask = 0;
                self->tapValid = TRUE;
            }
        }
        return FALSE;
    }

    if (bit) {
        if (bit != IBUS_LOCK_MASK) {
            if (self->tapMask == 0) self->tapValid = TRUE;
            self->tapMask |= bit;
        }
        return FALSE;
    }
    self->tapValid = FALSE;

    // Keys typed while a correction is in flight wait their turn.
    if (isPending(self)) {
        self->queue->push_back({keyval, keycode, state, false});
        return TRUE;
    }
    int result = handleKeyPress(self, keyval, keycode, state);
    if (result == KeyPassAfterPending) {
        self->queue->push_back({keyval, keycode, state, true});
        return TRUE;
    }
    return result == KeyPass ? FALSE : TRUE;
}

/*------------------------------------------------------------------------------*/
// IBusEngine overrides

static void omnikey_engine_focus_in(IBusEngine* engine) {
    OmnikeyEngine* self = (OmnikeyEngine*)engine;
    resetWord(self);
    ibus_engine_register_properties(engine, self->props);
    updateModeProperty(self);
    IBUS_ENGINE_CLASS(omnikey_engine_parent_class)->focus_in(engine);
}

static void omnikey_engine_focus_out(IBusEngine* engine) {
    OmnikeyEngine* self = (OmnikeyEngine*)engine;
    cancelPending(self);
    flushWord(self);
    resetWord(self);
    IBUS_ENGINE_CLASS(omnikey_engine_parent_class)->focus_out(engine);
}

static void omnikey_engine_reset(IBusEngine* engine) {
    // The client keeps the preedit (IBUS_ENGINE_PREEDIT_COMMIT); just forget the word.
    cancelPending((OmnikeyEngine*)engine);
    resetWord((OmnikeyEngine*)engine);
    IBUS_ENGINE_CLASS(omnikey_engine_parent_class)->reset(engine);
}

// The app reports the text around the cursor after every change. When the
// text before the cursor stops ending with what we typed, the user clicked or
// moved elsewhere and the half-typed word must not be edited any more. Updates
// can lag behind typing, so the check runs once things have settled.
static bool cursorFollowsTyping(OmnikeyEngine* self) {
    std::u32string before;
    bool selection = false;
    if (!textBeforeCursor(self, before, selection)) return true;
    // Apps may report only part of the text (one paragraph, say).
    const auto& recent = *self->recent;
    size_t length = MIN(recent.size(), before.size());
    if (length == 0) return recent.empty();
    return endsWith(before, recent.substr(recent.size() - length));
}

static gboolean onCursorCheck(gpointer data) {
    OmnikeyEngine* self = (OmnikeyEngine*)data;
    self->cursorCheckTimer = 0;
    if (vOutputMode == OutputDirect && !isPending(self) && !self->recent->empty() && !cursorFollowsTyping(self))
        resetWord(self);
    return G_SOURCE_REMOVE;
}

static void omnikey_engine_set_surrounding_text(IBusEngine* engine, IBusText* text, guint cursor, guint anchor) {
    IBUS_ENGINE_CLASS(omnikey_engine_parent_class)->set_surrounding_text(engine, text, cursor, anchor);
    OmnikeyEngine* self = (OmnikeyEngine*)engine;
    if (vOutputMode != OutputDirect || self->recent->empty() || self->cursorCheckTimer) return;
    if (!cursorFollowsTyping(self)) self->cursorCheckTimer = g_timeout_add(CURSOR_CHECK_MS, onCursorCheck, self);
}

static void omnikey_engine_enable(IBusEngine* engine) {
    // Asking once makes IBus keep the surrounding text up to date for direct mode.
    ibus_engine_get_surrounding_text(engine, NULL, NULL, NULL);
    IBUS_ENGINE_CLASS(omnikey_engine_parent_class)->enable(engine);
}

static void omnikey_engine_property_activate(IBusEngine* engine, const gchar* name, guint state) {
    OmnikeyEngine* self = (OmnikeyEngine*)engine;
    if (g_strcmp0(name, "InputMode") == 0) {
        cycleMode(self);
    } else if (g_strcmp0(name, "Setup") == 0) {
        g_spawn_command_line_async(OMNIKEY_SETUP_PATH, NULL);
    } else {
        for (int i = 0; i < ModeCount; i++) {
            if (g_strcmp0(name, MODE_KEYS[i]) == 0 && state == PROP_STATE_CHECKED) setMode(self, i);
        }
    }
}

static void omnikey_engine_candidate_clicked(IBusEngine* engine, guint index, guint button, guint state) {
    OmnikeyEngine* self = (OmnikeyEngine*)engine;
    if (!vChineseMode || !rime) return;
    if (rime->select(index)) commitChinese(self);
    showComposition(self);
}

static void omnikey_engine_page_up(IBusEngine* engine) {
    processChineseKey((OmnikeyEngine*)engine, IBUS_KEY_Page_Up);
}

static void omnikey_engine_page_down(IBusEngine* engine) {
    processChineseKey((OmnikeyEngine*)engine, IBUS_KEY_Page_Down);
}

static void omnikey_engine_cursor_up(IBusEngine* engine) {
    processChineseKey((OmnikeyEngine*)engine, IBUS_KEY_Up);
}

static void omnikey_engine_cursor_down(IBusEngine* engine) {
    processChineseKey((OmnikeyEngine*)engine, IBUS_KEY_Down);
}

static void omnikey_engine_init(OmnikeyEngine* self) {
    self->word = new std::vector<std::u32string>();
    self->sentLengths = new std::vector<int>();
    self->recent = new std::u32string();
    self->pendingText = new std::u32string();
    self->pendingPresses = self->pendingReleases = 0;
    self->pendingTimer = 0;
    self->queue = new std::vector<QueuedKey>();
    self->passPresses = new std::vector<guint>();
    self->passReleases = new std::vector<guint>();
    self->batch = new std::u32string();
    self->batching = false;
    self->cursorCheckTimer = 0;
    self->tapMask = 0;
    self->tapValid = TRUE;

    self->props = ibus_prop_list_new();
    g_object_ref_sink(self->props);
    IBusPropList* modes = ibus_prop_list_new();
    for (int i = 0; i < ModeCount; i++) {
        self->modeItems[i] = ibus_property_new(MODE_KEYS[i], PROP_TYPE_RADIO,
            ibus_text_new_from_static_string(MODE_LABELS[i]), NULL, NULL, TRUE, TRUE, PROP_STATE_UNCHECKED, NULL);
        ibus_prop_list_append(modes, self->modeItems[i]);
    }
    self->modeProp = ibus_property_new("InputMode", PROP_TYPE_MENU,
        ibus_text_new_from_static_string(MODE_LABELS[ModeVietnamese]), NULL,
        ibus_text_new_from_static_string("Chuyển chế độ gõ"), TRUE, TRUE, PROP_STATE_UNCHECKED, modes);
    g_object_ref_sink(self->modeProp);
    ibus_property_set_symbol(self->modeProp, ibus_text_new_from_static_string(MODE_SYMBOLS[ModeVietnamese]));
    ibus_prop_list_append(self->props, self->modeProp);
    ibus_prop_list_append(self->props, ibus_property_new("Setup", PROP_TYPE_NORMAL,
        ibus_text_new_from_static_string("Cài đặt Omnikey…"), "preferences-system",
        NULL, TRUE, TRUE, PROP_STATE_UNCHECKED, NULL));

    engines = g_list_prepend(engines, self);
}

static void omnikey_engine_destroy(IBusObject* object) {
    OmnikeyEngine* self = (OmnikeyEngine*)object;
    engines = g_list_remove(engines, self);
    g_clear_object(&self->modeProp);
    g_clear_object(&self->props);
    if (self->pendingTimer) g_source_remove(self->pendingTimer);
    if (self->cursorCheckTimer) g_source_remove(self->cursorCheckTimer);
    self->pendingTimer = self->cursorCheckTimer = 0;
    delete self->word;
    delete self->sentLengths;
    delete self->recent;
    delete self->pendingText;
    delete self->queue;
    delete self->passPresses;
    delete self->passReleases;
    delete self->batch;
    self->word = NULL;
    IBUS_OBJECT_CLASS(omnikey_engine_parent_class)->destroy(object);
}

static void omnikey_engine_class_init(OmnikeyEngineClass* klass) {
    IBusObjectClass* objectClass = IBUS_OBJECT_CLASS(klass);
    IBusEngineClass* engineClass = IBUS_ENGINE_CLASS(klass);
    objectClass->destroy = omnikey_engine_destroy;
    engineClass->process_key_event = omnikey_engine_process_key_event;
    engineClass->focus_in = omnikey_engine_focus_in;
    engineClass->focus_out = omnikey_engine_focus_out;
    engineClass->reset = omnikey_engine_reset;
    engineClass->enable = omnikey_engine_enable;
    engineClass->set_surrounding_text = omnikey_engine_set_surrounding_text;
    engineClass->property_activate = omnikey_engine_property_activate;
    engineClass->candidate_clicked = omnikey_engine_candidate_clicked;
    engineClass->page_up = omnikey_engine_page_up;
    engineClass->page_down = omnikey_engine_page_down;
    engineClass->cursor_up = omnikey_engine_cursor_up;
    engineClass->cursor_down = omnikey_engine_cursor_down;
}

/*------------------------------------------------------------------------------*/
// Settings

static void reloadConfig() {
    OmnikeyConfig::load();
    if (loadedCodeTable != vCodeTable) {
        loadedCodeTable = vCodeTable;
        onTableCodeChange();
    }
    if (vChineseMode) startChinese();
    for (GList* it = engines; it; it = it->next) {
        resetWord((OmnikeyEngine*)it->data);
        updateModeProperty((OmnikeyEngine*)it->data);
    }
}

static void onConfigChanged(GFileMonitor*, GFile* file, GFile* otherFile, GFileMonitorEvent event, gpointer) {
    if (event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT && event != G_FILE_MONITOR_EVENT_CREATED &&
        event != G_FILE_MONITOR_EVENT_RENAMED && event != G_FILE_MONITOR_EVENT_MOVED_IN) return;
    if (event == G_FILE_MONITOR_EVENT_RENAMED) file = otherFile; // saved through a temporary file
    gchar* name = g_file_get_basename(file);
    if (g_strcmp0(name, "omnikey.conf") == 0) reloadConfig();
    else if (g_strcmp0(name, "macros.txt") == 0) OmnikeyConfig::loadMacros();
    g_free(name);
}

void omnikey_engine_global_init(void) {
    pData = (vKeyHookState*)vKeyInit();
    if (!VirtualKeyboard::open())
        g_warning("Omnikey: /dev/uinput is not accessible; edit-in-place mode falls back to IBus key forwarding");
    OmnikeyConfig::load();
    loadedCodeTable = vCodeTable;
    OmnikeyConfig::loadMacros();
    if (vChineseMode) startChinese();

    g_mkdir_with_parents(OmnikeyConfig::configDir().c_str(), 0700);
    GFile* dir = g_file_new_for_path(OmnikeyConfig::configDir().c_str());
    configMonitor = g_file_monitor_directory(dir, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
    if (configMonitor) g_signal_connect(configMonitor, "changed", G_CALLBACK(onConfigChanged), NULL);
    g_object_unref(dir);
}
