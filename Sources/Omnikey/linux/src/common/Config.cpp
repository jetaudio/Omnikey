//
//  Config.cpp
//  Omnikey - Linux
//

#include "Config.h"
#include "Engine.h"

#include <glib.h>
#include <glib/gstdio.h>

// Engine globals (the engine expects the host to define them).
int vLanguage = 1;
int vInputType = vTelex;
int vFreeMark = 0;
int vCodeTable = 0;
int vSwitchKeyStatus = 0;
int vCheckSpelling = 1;
int vUseModernOrthography = 1;
int vQuickTelex = 0;
int vRestoreIfWrongSpelling = 1;
int vFixRecommendBrowser = 0;
int vUseMacro = 1;
int vUseMacroInEnglishMode = 0;
int vAutoCapsMacro = 0;
int vUseSmartSwitchKey = 0;
int vUpperCaseFirstChar = 0;
int vTempOffSpelling = 0;
int vAllowConsonantZFWJ = 0;
int vQuickStartConsonant = 0;
int vQuickEndConsonant = 0;
int vRememberCode = 0;
int vOtherLanguage = 0;
int vTempOffOmnikey = 0;

int vOutputMode = OutputPreedit;
int vSwitchKey = SwitchCtrlShift;

static const char* GROUP = "Omnikey";

static const struct { const char* key; int* value; } SETTINGS[] = {
    {"Language", &vLanguage},
    {"InputType", &vInputType},
    {"CodeTable", &vCodeTable},
    {"FreeMark", &vFreeMark},
    {"CheckSpelling", &vCheckSpelling},
    {"ModernOrthography", &vUseModernOrthography},
    {"QuickTelex", &vQuickTelex},
    {"RestoreIfWrongSpelling", &vRestoreIfWrongSpelling},
    {"UseMacro", &vUseMacro},
    {"AutoCapsMacro", &vAutoCapsMacro},
    {"UpperCaseFirstChar", &vUpperCaseFirstChar},
    {"TempOffSpelling", &vTempOffSpelling},
    {"AllowConsonantZFWJ", &vAllowConsonantZFWJ},
    {"QuickStartConsonant", &vQuickStartConsonant},
    {"QuickEndConsonant", &vQuickEndConsonant},
    {"TempOffOmnikey", &vTempOffOmnikey},
    {"OutputMode", &vOutputMode},
    {"SwitchKey", &vSwitchKey},
};

namespace OmnikeyConfig {

std::string configDir() {
    gchar* dir = g_build_filename(g_get_user_config_dir(), "omnikey", NULL);
    std::string result(dir);
    g_free(dir);
    return result;
}

std::string configPath() {
    return configDir() + "/omnikey.conf";
}

std::string macroPath() {
    return configDir() + "/macros.txt";
}

// Copies the settings of the app from before it was renamed from OpenKey.
static void migrateFromOpenKey() {
    static bool done = false;
    if (done) return;
    done = true;
    if (g_file_test(configDir().c_str(), G_FILE_TEST_EXISTS)) return;
    gchar* legacy = g_build_filename(g_get_user_config_dir(), "openkey", NULL);
    const char* files[][2] = {{"openkey.conf", "omnikey.conf"}, {"macros.txt", "macros.txt"}};
    for (const auto& names : files) {
        gchar* from = g_build_filename(legacy, names[0], NULL);
        gchar* contents = NULL;
        gsize length = 0;
        if (g_file_get_contents(from, &contents, &length, NULL)) {
            g_mkdir_with_parents(configDir().c_str(), 0700);
            g_file_set_contents((configDir() + "/" + names[1]).c_str(), contents, (gssize)length, NULL);
            g_free(contents);
        }
        g_free(from);
    }
    g_free(legacy);
}

void load() {
    migrateFromOpenKey();
    GKeyFile* file = g_key_file_new();
    if (g_key_file_load_from_file(file, configPath().c_str(), G_KEY_FILE_NONE, NULL)) {
        for (const auto& setting : SETTINGS) {
            GError* error = NULL;
            int value = g_key_file_get_integer(file, GROUP, setting.key, &error);
            if (error) g_error_free(error);
            else *setting.value = value;
        }
    }
    g_key_file_free(file);

    if (vInputType < vTelex || vInputType > vSimpleTelex2) vInputType = vTelex;
    if (vCodeTable < 0 || vCodeTable > 4) vCodeTable = 0;
    if (vSwitchKey < 0 || vSwitchKey >= SwitchKeyCount) vSwitchKey = SwitchCtrlShift;
    vLanguage = vLanguage ? 1 : 0;
    vSetCheckSpelling();
}

void save() {
    g_mkdir_with_parents(configDir().c_str(), 0700);
    GKeyFile* file = g_key_file_new();
    for (const auto& setting : SETTINGS) {
        g_key_file_set_integer(file, GROUP, setting.key, *setting.value);
    }
    g_key_file_save_to_file(file, configPath().c_str(), NULL);
    g_key_file_free(file);
}

void loadMacros() {
    readFromFile(macroPath(), false);
}

void saveMacros() {
    g_mkdir_with_parents(configDir().c_str(), 0700);
    saveToFile(macroPath());
}

}
