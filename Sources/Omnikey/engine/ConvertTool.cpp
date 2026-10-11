//
//  ConvertTool.cpp
//  OpenKey
//
//  Created by Tuyen on 9/4/19.
//  Copyright © 2019 Tuyen Mai. All rights reserved.
//
#include <locale>
#include <codecvt>
#include "ConvertTool.h"
#include "Engine.h"
#include <iostream>
#include <memory.h>
#include <algorithm>

//option
bool convertToolDontAlertWhenCompleted = false;
bool convertToolToAllCaps = false;
bool convertToolToAllNonCaps = false;
bool convertToolToCapsFirstLetter = false;
bool convertToolToCapsEachWord = false;
bool convertToolRemoveMark = false;
Uint8 convertToolFromCode = 0;
Uint8 convertToolToCode = 0;
int convertToolHotKey = 0;

static vector<Uint8> _breakCode = {'.', '?', '!'};

static bool findKeyCode(const Uint32& charCode, const Uint8& code, int& j, int& k) {
    //find character which has tone/mark
    for (map<Uint32, vector<Uint16>>::iterator it = _codeTable[code].begin(); it != _codeTable[code].end(); ++it) {
        for (int z = 0; z < it->second.size(); z++) {
            if (charCode == it->second[z]) {
                j = it->first;
                k = z;
                // TCVN3 uses the same byte for many upper/lowercase glyphs;
                // without font information, decode those ambiguous bytes as lowercase.
                if (code == 1 && z % 2 == 0 && z + 1 < it->second.size() &&
                    it->second[z] == it->second[z + 1]) ++k;
                return true;
            }//end if
        }
    }
    return false;
}

static Uint16 getUnicodeCompoundMarkIndex(const Uint16& mark) {
    for (int i = 0; i < 5; i++) {
        if (mark == _unicodeCompoundMark[i]) {
            return ((i + 1) << 13);
        }
    }
    return 0;
}

static Uint16 convertedCharacter(int row, int column, bool shouldUpperCase) {
    const bool upper = !convertToolToAllNonCaps && (convertToolToAllCaps || shouldUpperCase);
    const bool lower = convertToolToAllNonCaps ||
        (!upper && (convertToolToCapsFirstLetter || convertToolToCapsEachWord));
    if (upper && column % 2 != 0) --column;
    else if (lower && column % 2 == 0) ++column;

    if (convertToolRemoveMark) {
        // Even columns are uppercase; preserve that case unless explicitly changed.
        return keyCodeToCharacter((Uint8)row | (column % 2 == 0 ? CAPS_MASK : 0));
    }
    return _codeTable[convertToolToCode][row][column];
}

static void appendEncodedCharacter(vector<wchar_t>& out, Uint16 character) {
    if (convertToolToCode == 2 || convertToolToCode == 4) {
        out.push_back(LOBYTE(character));
        if (HIBYTE(character) > 32) out.push_back(HIBYTE(character));
    } else if (convertToolToCode == 3 && (character >> 13) > 0) {
        out.push_back(character & 0x1FFF);
        out.push_back(_unicodeCompoundMark[(character >> 13) - 1]);
    } else {
        out.push_back(character);
    }
}

string convertUtil(const string& sourceString) {
    if (convertToolFromCode >= 5 || convertToolToCode >= 5) return sourceString;
    wstring data = utf8ToWideString(sourceString);
    Uint16 t = 0, target;
    int j, k, p;
    vector<wchar_t> _temp;
    bool hasBreak = false;
    bool shouldUpperCase = false;
    if (convertToolToCapsFirstLetter || convertToolToCapsEachWord)
        shouldUpperCase = true;
    if (convertToolToAllNonCaps)
        shouldUpperCase = false;
    
    for (int i = 0; i < data.size(); i++) {
        p = 0;
        //find char with tone/mark
        if (i < data.size() - 1) {
            switch (convertToolFromCode) {
                case 2: //VNI
                case 4: //1258
                    t = (Uint16)data[i];
                    if (data[i] <= 0xFF && data[i+1] <= 0xFF) {
                        t |= data[i+1] << 8;
                        p = 1;
                    }
                    break;
                case 3:{ //Unicode Compound
                    target = getUnicodeCompoundMarkIndex(data[i+1]);
                    if (target > 0){
                        t = (Uint16)data[i] | target;
                        p = 1;
                    } else {
                        t = (Uint16)data[i];
                    }
                    break;
                }
                default:
                    t = (Uint16)data[i];
                    break;
            }
            
            if (findKeyCode(t, convertToolFromCode, j, k)) {
                i += p;
                target = convertedCharacter(j, k, shouldUpperCase);
                appendEncodedCharacter(_temp, target);
                shouldUpperCase = false;
                hasBreak = false;
                continue;
            }
        }
        
        //find primary keycode first
        t = (Uint16)data[i];
        if (findKeyCode(t, convertToolFromCode, j, k)) {
            target = convertedCharacter(j, k, shouldUpperCase);
            appendEncodedCharacter(_temp, target);
            shouldUpperCase = false;
            hasBreak = false;
            continue;
        }
        
        //if dont find => normal char
        if (!convertToolToAllNonCaps && (convertToolToAllCaps || shouldUpperCase))
            _temp.push_back(towupper(data[i]));
        else if (convertToolToAllNonCaps || convertToolToCapsFirstLetter || convertToolToCapsEachWord)
            _temp.push_back(towlower(data[i]));
        else
            _temp.push_back(data[i]);
        
        if (t == '\n' || (hasBreak && t == ' ')) {
            if (convertToolToCapsFirstLetter || convertToolToCapsEachWord)
                shouldUpperCase = true;
        } else if (t == ' ' && convertToolToCapsEachWord) {
            shouldUpperCase = true;
        } else if (std::find(_breakCode.begin(), _breakCode.end(), t) != _breakCode.end()) {
            hasBreak = true;
        } else {
            shouldUpperCase = false;
            hasBreak = false;
        }
    }
    _temp.push_back(0);
    wstring str(_temp.begin(), _temp.end());
    return wideStringToUtf8(str);
}
