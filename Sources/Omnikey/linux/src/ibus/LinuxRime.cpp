//
//  LinuxRime.cpp
//  Omnikey - Linux IBus engine
//

#include "LinuxRime.h"
#include "RimeCustom.h"

#include <ibus.h>
#include <rime_api.h>

// Rime uses the X11 modifier bits.
#define RIME_SHIFT_MASK (1 << 0)
#define RIME_CONTROL_MASK (1 << 2)
#define RIME_ALT_MASK (1 << 3)

const char* const LinuxRime::kSharedDataDir = "/usr/share/rime-data";

static RimeApi* api() {
    static RimeApi* rime = rime_get_api();
    return rime;
}

LinuxRime::~LinuxRime() {
    if (session_) api()->destroy_session(session_);
    if (initialized_) api()->finalize();
}

bool LinuxRime::initialize(const std::string& shared, const std::string& user) {
    if (session_) return true;
    if (initialized_) return false;
    RimeApi* rime = api();
    if (!rime || !RIME_API_AVAILABLE(rime, select_candidate_on_current_page)) {
        error_ = "Thư viện librime không tương thích.";
        return false;
    }
    std::string dictionary = shared + "/build/pinyin_simp.table.bin";
    if (!g_file_test(dictionary.c_str(), G_FILE_TEST_EXISTS)) {
        error_ = "Thiếu dữ liệu Pinyin; cài gói rime-data-pinyin-simp.";
        return false;
    }
    if (g_mkdir_with_parents(user.c_str(), 0700) != 0) {
        error_ = "Không tạo được thư mục dữ liệu Rime của người dùng.";
        return false;
    }
    // Omnikey's Rime settings, shared with macOS and Windows.
    std::string custom = user + "/default.custom.yaml";
    gchar* current = NULL;
    bool same = g_file_get_contents(custom.c_str(), &current, NULL, NULL) && g_strcmp0(current, kRimeCustomYaml) == 0;
    g_free(current);
    if (!same && !g_file_set_contents(custom.c_str(), kRimeCustomYaml, -1, NULL)) {
        error_ = "Không ghi được cấu hình Rime của Omnikey.";
        return false;
    }

    shared_ = shared;
    user_ = user;
    prebuilt_ = shared + "/build";
    staging_ = user + "/build";
    RIME_STRUCT(RimeTraits, traits);
    traits.shared_data_dir = shared_.c_str();
    traits.user_data_dir = user_.c_str();
    traits.prebuilt_data_dir = prebuilt_.c_str();
    traits.staging_dir = staging_.c_str();
    traits.distribution_name = "Omnikey";
    traits.distribution_code_name = "Omnikey";
    traits.distribution_version = "1";
    traits.app_name = "rime.omnikey";
    traits.min_log_level = 2;
    traits.log_dir = "";
    rime->setup(&traits);
    rime->initialize(&traits);
    initialized_ = true;
    if (rime->start_maintenance(False)) rime->join_maintenance_thread();
    session_ = rime->create_session();
    if (!session_ || !rime->select_schema(session_, "pinyin_simp")) {
        error_ = "Không khởi tạo được bộ gõ Pinyin.";
        if (session_) rime->destroy_session(session_);
        session_ = 0;
        return false;
    }
    rime->set_option(session_, "ascii_mode", False);
    return true;
}

bool LinuxRime::process(int keysym, int mask) {
    return session_ && api()->process_key(session_, keysym, mask);
}

bool LinuxRime::select(size_t index) {
    return session_ && api()->select_candidate_on_current_page(session_, index);
}

void LinuxRime::clear() {
    if (session_) api()->clear_composition(session_);
}

bool LinuxRime::composing() {
    RIME_STRUCT(RimeStatus, status);
    if (!session_ || !api()->get_status(session_, &status)) return false;
    bool result = status.is_composing;
    api()->free_status(&status);
    return result;
}

std::string LinuxRime::takeCommit() {
    RIME_STRUCT(RimeCommit, commit);
    if (!session_ || !api()->get_commit(session_, &commit)) return {};
    std::string result = commit.text ? commit.text : "";
    api()->free_commit(&commit);
    return result;
}

ChineseComposition LinuxRime::composition() {
    ChineseComposition result;
    RIME_STRUCT(RimeContext, context);
    if (!session_ || !api()->get_context(session_, &context)) return result;
    if (context.composition.preedit) {
        result.preedit = context.composition.preedit;
        int bytes = MIN(MAX(context.composition.cursor_pos, 0), (int)result.preedit.size());
        result.cursor = (int)g_utf8_strlen(result.preedit.c_str(), bytes);
    }
    result.highlighted = context.menu.highlighted_candidate_index;
    for (int i = 0; i < context.menu.num_candidates; ++i) {
        const char* text = context.menu.candidates[i].text;
        const char* comment = context.menu.candidates[i].comment;
        result.candidates.push_back(text ? text : "");
        result.comments.push_back(comment ? comment : "");
    }
    api()->free_context(&context);
    return result;
}

bool LinuxRime::keyFor(unsigned keyval, unsigned state, bool composing, int& keysym, int& mask) {
    if (state & (IBUS_SUPER_MASK | IBUS_MOD4_MASK | IBUS_HYPER_MASK | IBUS_META_MASK)) return false;
    // Caps Lock types Latin letters, as in the system Pinyin input.
    if ((state & IBUS_LOCK_MASK) && !composing) return false;
    int modifiers = 0;
    if (state & IBUS_CONTROL_MASK) modifiers |= RIME_CONTROL_MASK;
    if (state & IBUS_MOD1_MASK) modifiers |= RIME_ALT_MASK;
    // Shift+Delete removes the highlighted phrase from what Rime learned.
    int shift = (state & IBUS_SHIFT_MASK) ? RIME_SHIFT_MASK : 0;
    mask = modifiers;
    switch (keyval) {
        case IBUS_KEY_BackSpace: keysym = IBUS_KEY_BackSpace; mask |= shift; return true;
        case IBUS_KEY_Delete: case IBUS_KEY_KP_Delete: keysym = IBUS_KEY_Delete; mask |= shift; return true;
        case IBUS_KEY_Tab: case IBUS_KEY_ISO_Left_Tab: keysym = IBUS_KEY_Tab; mask |= shift; return true;
        case IBUS_KEY_Return: case IBUS_KEY_KP_Enter: keysym = IBUS_KEY_Return; return true;
        case IBUS_KEY_Escape: keysym = IBUS_KEY_Escape; return true;
        case IBUS_KEY_Left: case IBUS_KEY_KP_Left: keysym = IBUS_KEY_Left; return true;
        case IBUS_KEY_Up: case IBUS_KEY_KP_Up: keysym = IBUS_KEY_Up; return true;
        case IBUS_KEY_Right: case IBUS_KEY_KP_Right: keysym = IBUS_KEY_Right; return true;
        case IBUS_KEY_Down: case IBUS_KEY_KP_Down: keysym = IBUS_KEY_Down; return true;
        case IBUS_KEY_Page_Up: case IBUS_KEY_KP_Page_Up: keysym = IBUS_KEY_Page_Up; return true;
        case IBUS_KEY_Page_Down: case IBUS_KEY_KP_Page_Down: keysym = IBUS_KEY_Page_Down; return true;
        case IBUS_KEY_Home: case IBUS_KEY_KP_Home: keysym = IBUS_KEY_Home; return true;
        case IBUS_KEY_End: case IBUS_KEY_KP_End: keysym = IBUS_KEY_End; return true;
        case IBUS_KEY_space: case IBUS_KEY_KP_Space: keysym = IBUS_KEY_space; return true;
        default: break;
    }
    if (state & IBUS_CONTROL_MASK) {
        if (!composing) return false;
        unsigned lower = (keyval >= IBUS_KEY_A && keyval <= IBUS_KEY_Z) ? keyval + (IBUS_KEY_a - IBUS_KEY_A) : keyval;
        if (lower < IBUS_KEY_a || lower > IBUS_KEY_z) return false;
        keysym = (int)lower;
        return true;
    }
    // Printable keys: the keyval already carries Shift and the layout.
    gunichar ch = ibus_keyval_to_unicode(keyval);
    if (ch > 0x20 && ch < 0x7f) {
        keysym = (int)ch;
        return true;
    }
    return false;
}
