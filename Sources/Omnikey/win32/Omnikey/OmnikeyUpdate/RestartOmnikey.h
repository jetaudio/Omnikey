#pragma once

#include <windows.h>
#include <string>

// Use an explicit executable and working directory: the updater itself runs
// from a temporary directory, and installation paths may contain spaces.
inline bool RestartOmnikey(const wchar_t* directory, const wchar_t* mainExe)
{
    std::wstring executable(directory);
    if (!executable.empty() && executable.back() != L'\\') executable += L'\\';
    executable += mainExe;
    std::wstring command = L"\"" + executable + L"\"";
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr,
        FALSE, 0, nullptr, directory, &startup, &process)) return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}
