//
//  LinuxRime.h
//  Omnikey - Linux IBus engine
//
//  Chinese (simplified Pinyin) input through librime, with the schema data
//  from Ubuntu's rime-data packages. One session serves every input context.
//

#ifndef OMNIKEY_LINUX_RIME_H
#define OMNIKEY_LINUX_RIME_H

#include <cstdint>
#include <string>
#include <vector>

struct ChineseComposition {
    std::string preedit;
    int cursor = 0; // in characters
    std::vector<std::string> candidates;
    std::vector<std::string> comments;
    int highlighted = 0;
    bool composing() const { return !preedit.empty() || !candidates.empty(); }
};

class LinuxRime {
public:
    static const char* const kSharedDataDir;

    LinuxRime() = default;
    ~LinuxRime();
    // Blocks while Rime deploys its data, so the engine runs it off the main loop.
    bool initialize(const std::string& shared, const std::string& user);
    bool process(int keysym, int mask = 0);
    bool select(size_t index);
    void clear();
    bool composing();
    std::string takeCommit();
    ChineseComposition composition();
    const std::string& error() const { return error_; }

    // Turns an IBus key press into the keysym and modifiers Rime expects.
    // False when the key belongs to the app: Super shortcuts, Caps Lock Latin
    // letters, and Ctrl shortcuts unless a composition is open.
    static bool keyFor(unsigned keyval, unsigned state, bool composing, int& keysym, int& mask);

private:
    uintptr_t session_ = 0;
    bool initialized_ = false;
    std::string error_;
    std::string shared_, user_, prebuilt_, staging_;
    LinuxRime(const LinuxRime&) = delete;
    LinuxRime& operator=(const LinuxRime&) = delete;
};

#endif
