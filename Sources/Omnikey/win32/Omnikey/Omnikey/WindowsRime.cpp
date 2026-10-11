#include "WindowsRime.h"
#include <shlobj.h>

static std::string utf8(const std::wstring& text) {
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    if (size) WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), &result[0], size, nullptr, nullptr);
    return result;
}

static std::wstring wide(const char* text) {
    if (!text) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::wstring result(size, L'\0');
    if (size) MultiByteToWideChar(CP_UTF8, 0, text, -1, &result[0], size);
    if (!result.empty()) result.pop_back();
    return result;
}

WindowsRime::~WindowsRime() {
    if (session_) api_->destroy_session(session_);
    if (initialized_) api_->finalize();
    if (library_) FreeLibrary(library_);
}

bool WindowsRime::initialize(const std::wstring& bundle, const std::wstring& user) {
    if (session_) return true;
    if (library_) return false;
    std::wstring dictionary = bundle + L"\\build\\pinyin_simp.table.bin";
    if (GetFileAttributesW(dictionary.c_str()) == INVALID_FILE_ATTRIBUTES) {
        error_ = L"Thiếu từ điển Pinyin trong thư mục Rime cạnh Omnikey.exe.";
        return false;
    }
    // The combined release keeps architecture-specific DLLs under one Rime folder.
#ifdef _WIN64
    std::wstring library = bundle + L"\\bin\\x64\\rime.dll";
#else
    std::wstring library = bundle + L"\\bin\\x86\\rime.dll";
#endif
    if (GetFileAttributesW(library.c_str()) == INVALID_FILE_ATTRIBUTES) library = bundle + L"\\bin\\rime.dll";
    library_ = LoadLibraryExW(library.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!library_) {
        error_ = L"Không tải được Rime\\bin\\rime.dll (mã lỗi " + std::to_wstring(GetLastError()) + L").";
        return false;
    }
    auto getApi = reinterpret_cast<RimeApi* (*)()>(GetProcAddress(library_, "rime_get_api"));
    api_ = getApi ? getApi() : nullptr;
    if (!api_ || !RIME_API_AVAILABLE(api_, select_candidate_on_current_page)) {
        error_ = L"Thư viện Rime không tương thích.";
        return false;
    }
    int created = SHCreateDirectoryExW(nullptr, user.c_str(), nullptr);
    if (created != ERROR_SUCCESS && created != ERROR_ALREADY_EXISTS && created != ERROR_FILE_EXISTS) {
        error_ = L"Không tạo được thư mục dữ liệu Rime của người dùng.";
        return false;
    }
    shared_ = utf8(bundle + L"\\shared");
    user_ = utf8(user);
    prebuilt_ = utf8(bundle + L"\\build");
    staging_ = utf8(user + L"\\build");
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
    api_->setup(&traits);
    api_->initialize(&traits);
    initialized_ = true;
    if (api_->start_maintenance(False)) api_->join_maintenance_thread();
    session_ = api_->create_session();
    if (!session_ || !api_->select_schema(session_, "pinyin_simp")) {
        error_ = L"Không khởi tạo được bộ gõ Pinyin.";
        if (session_) api_->destroy_session(session_);
        session_ = 0;
        return false;
    }
    api_->set_option(session_, "ascii_mode", False);
    return true;
}

bool WindowsRime::process(int keysym, int mask) {
    return session_ && api_->process_key(session_, keysym, mask);
}

bool WindowsRime::select(size_t index) {
    return session_ && api_->select_candidate_on_current_page(session_, index);
}

void WindowsRime::clear() {
    if (session_) api_->clear_composition(session_);
}

std::wstring WindowsRime::takeCommit() {
    RIME_STRUCT(RimeCommit, commit);
    if (!session_ || !api_->get_commit(session_, &commit)) return {};
    auto result = wide(commit.text);
    api_->free_commit(&commit);
    return result;
}

ChineseComposition WindowsRime::composition() {
    ChineseComposition result;
    RIME_STRUCT(RimeContext, context);
    if (!session_ || !api_->get_context(session_, &context)) return result;
    result.preedit = wide(context.composition.preedit);
    result.highlighted = context.menu.highlighted_candidate_index;
    result.page = context.menu.page_no;
    result.lastPage = context.menu.is_last_page != 0;
    result.selectLabels = wide(context.menu.select_keys);
    for (int i = 0; i < context.menu.num_candidates; ++i) {
        result.candidates.push_back(wide(context.menu.candidates[i].text));
        result.comments.push_back(wide(context.menu.candidates[i].comment));
    }
    api_->free_context(&context);
    return result;
}
