//
//  SmartSwitchKey.cpp
//  OpenKey
//
//  Created by Tuyen on 8/13/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//

#include "SmartSwitchKey.h"
#include <map>
#include <iostream>
#include <memory.h>

//main data, i use `map` because it has O(Log(n))
static map<string, Int8> _smartSwitchKeyData;
static string _cacheKey = ""; //use cache for faster
static Int8 _cacheData = 0; //use cache for faster

void initSmartSwitchKey(const Byte* pData, const int& size) {
    _smartSwitchKeyData.clear();
    _cacheKey.clear();
    _cacheData = 0;
    if (pData == NULL) return;
    Uint16 count = 0;
    Uint32 cursor = 0;
    if (size >= 2) {
        memcpy(&count, pData + cursor, 2);
        cursor+=2;
    }
    Uint8 bundleIdSize;
    Uint8 value;
    for (int i = 0; i < count; i++) {
        if (cursor >= (Uint32)size) break;
        bundleIdSize = pData[cursor++];
        if (cursor + bundleIdSize >= (Uint32)size) break;
        string bundleId((char*)pData + cursor, bundleIdSize);
        cursor += bundleIdSize;
        value = pData[cursor++];
        _smartSwitchKeyData[bundleId] = value;
    }
}

static bool isDefaultEnglishApp(const string& bundleId) {
#if !defined(_WIN32) && !defined(LINUX)
    static const vector<string> apps = {
        "com.apple.Terminal", "com.googlecode.iterm2", "com.microsoft.VSCode",
        "com.microsoft.VSCodeInsiders", "com.sublimetext.3", "com.sublimetext.4",
        "com.apple.dt.Xcode", "dev.zed.Zed", "com.mitchellh.ghostty",
        "io.alacritty", "org.alacritty", "com.github.wez.wezterm", "net.kovidgoyal.kitty"
    };
    for (const string& app : apps) if (app == bundleId) return true;
    return bundleId.rfind("com.jetbrains.", 0) == 0;
#else
    return false;
#endif
}

void getSmartSwitchKeySaveData(vector<Byte>& outData) {
    outData.clear();
    Uint16 count = (Uint16)_smartSwitchKeyData.size();
    outData.push_back((Byte)count);
    outData.push_back((Byte)(count>>8));
    
    for (std::map<string, Int8>::iterator it = _smartSwitchKeyData.begin(); it != _smartSwitchKeyData.end(); ++it) {
        outData.push_back((Byte)it->first.length());
        for (int j = 0; j < it->first.length(); j++) {
            outData.push_back(it->first[j]);
        }
        outData.push_back(it->second);
    }
}

int getAppInputMethodStatus(const string& bundleId, const int& currentInputMethod) {
    if (bundleId.empty()) return -1;
    if (_cacheKey.compare(bundleId) == 0) {
        return _cacheData;
    }
    map<string, Int8>::iterator it = _smartSwitchKeyData.find(bundleId);
    if (it != _smartSwitchKeyData.end()) {
        _cacheKey = bundleId;
        _cacheData = it->second;
        return _cacheData;
    }
    _cacheKey = bundleId;
    // Bit 0 is language; higher bits contain the remembered code table.
    const bool isEnglishApp = isDefaultEnglishApp(bundleId);
    _cacheData = isEnglishApp ? currentInputMethod & ~1 : currentInputMethod;
    _smartSwitchKeyData[bundleId] = _cacheData;
    return isEnglishApp ? _cacheData : -1;
}

void setAppInputMethodStatus(const string& bundleId, const int& language) {
    if (bundleId.empty()) return;
    _smartSwitchKeyData[bundleId] = language;
    _cacheKey = bundleId;
    _cacheData = language;
}
