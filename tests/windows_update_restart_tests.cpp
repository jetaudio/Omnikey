#include <windows.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static bool launchSucceeds = true;
static std::wstring application, commandLine, workingDirectory;
static std::vector<HANDLE> closedHandles;

static BOOL WINAPI TestCreateProcessW(LPCWSTR app, LPWSTR command,
    LPSECURITY_ATTRIBUTES processAttributes, LPSECURITY_ATTRIBUTES threadAttributes,
    BOOL inheritHandles, DWORD flags, LPVOID environment, LPCWSTR directory,
    LPSTARTUPINFOW startup, LPPROCESS_INFORMATION process)
{
    application = app;
    commandLine = command;
    workingDirectory = directory;
    assert(!processAttributes && !threadAttributes && !inheritHandles);
    assert(flags == 0 && !environment && startup->cb == sizeof(*startup));
    if (!launchSucceeds) return FALSE;
    process->hThread = reinterpret_cast<HANDLE>(1);
    process->hProcess = reinterpret_cast<HANDLE>(2);
    return TRUE;
}

static BOOL WINAPI TestCloseHandle(HANDLE handle)
{
    closedHandles.push_back(handle);
    return TRUE;
}

#define CreateProcessW TestCreateProcessW
#define CloseHandle TestCloseHandle
#include "../Sources/Omnikey/win32/Omnikey/OmnikeyUpdate/RestartOmnikey.h"
#undef CreateProcessW
#undef CloseHandle

int main()
{
    // Never launch an actual app. Capture the production Win32 call instead.
    const wchar_t* directory = L"C:\\Ứng dụng\\Omnikey cập nhật";
    for (const wchar_t* exe : { L"Omnikey64.exe", L"Omnikey32.exe" }) {
        closedHandles.clear();
        assert(RestartOmnikey(directory, exe));
        assert(application == std::wstring(directory) + L"\\" + exe);
        assert(commandLine == L"\"" + application + L"\"");
        assert(workingDirectory == directory);
        assert(closedHandles.size() == 2);
        assert(closedHandles[0] == reinterpret_cast<HANDLE>(1));
        assert(closedHandles[1] == reinterpret_cast<HANDLE>(2));
    }
    assert(RestartOmnikey(L"C:\\Omnikey\\", L"Omnikey64.exe"));
    assert(application == L"C:\\Omnikey\\Omnikey64.exe");
    launchSucceeds = false;
    closedHandles.clear();
    assert(!RestartOmnikey(directory, L"Omnikey64.exe"));
    assert(closedHandles.empty());
    std::cout << "Updater restart tests passed (x64/x86 targets, Unicode paths, quoting, failure, handles)\n";
}
