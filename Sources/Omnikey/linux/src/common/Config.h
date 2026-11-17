//
//  Config.h
//  Omnikey - Linux
//
//  Settings shared by the IBus engine and the setup app. They live in
//  ~/.config/omnikey/omnikey.conf (GKeyFile); macros in ~/.config/omnikey/macros.txt.
//

#ifndef OMNIKEY_LINUX_CONFIG_H
#define OMNIKEY_LINUX_CONFIG_H

#include <string>

// Linux-only settings that the shared engine does not know about.
enum OmnikeyOutputMode {
    OutputPreedit = 0,  // the word is shown underlined until it is finished; works everywhere
    OutputDirect = 1    // text is edited in place with surrounding text or backspaces
};

enum OmnikeySwitchKey {
    SwitchNone = 0,
    SwitchCtrlShift,    // tap Ctrl + Shift
    SwitchCtrlSpace,
    SwitchAltZ,
    SwitchKeyCount
};

extern int vOutputMode;
extern int vSwitchKey;
extern int vChineseMode; // 1 while 中 (Pinyin through Rime) is the input mode; vLanguage is then 0

namespace OmnikeyConfig {
    std::string configDir();
    std::string configPath();
    std::string macroPath();

    // Reads the config file into the engine globals; missing keys keep their defaults.
    void load();
    void save();
    // Loads the macro file into the engine macro table.
    void loadMacros();
    void saveMacros();
}

#endif
