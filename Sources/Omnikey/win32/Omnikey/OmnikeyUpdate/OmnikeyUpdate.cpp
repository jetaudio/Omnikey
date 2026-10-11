/*----------------------------------------------------------
OpenKey - The Cross platform Open source Vietnamese Keyboard application.

Copyright (C) 2019 Mai Vu Tuyen
Contact: maivutuyen.91@gmail.com
Github: https://github.com/tuyenvm/OpenKey
Fanpage: https://www.facebook.com/OpenKeyVN

This file is belong to the OpenKey project, Win32 version
which is released under GPL license.
You can fork, modify, improve this program. If you
redistribute your new version, it MUST be open source.
-----------------------------------------------------------*/

#include "framework.h"
#include "OmnikeyUpdate.h"
#include "RestartOmnikey.h"
#include <Urlmon.h>
#include <fstream>
#include <sstream>
#include <string>
#include <wincrypt.h>
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Crypt32.lib")

using namespace std;

INT_PTR CALLBACK MainDialogProcess(HWND, UINT, WPARAM, LPARAM);
void StartUpdate();
HWND hDlg;
const wchar_t* updateTarget = nullptr;
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    // Installs updated from OpenKey keep their old EXE name, which the startup
    // entries point to; update whichever name is present.
    if (wcscmp(lpCmdLine, L"--x64") == 0)
        updateTarget = GetFileAttributesW(L"Omnikey64.exe") == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW(L"OpenKey64.exe") != INVALID_FILE_ATTRIBUTES ? L"OpenKey64.exe" : L"Omnikey64.exe";
    else if (wcscmp(lpCmdLine, L"--x86") == 0)
        updateTarget = GetFileAttributesW(L"Omnikey32.exe") == INVALID_FILE_ATTRIBUTES &&
            GetFileAttributesW(L"OpenKey32.exe") != INVALID_FILE_ATTRIBUTES ? L"OpenKey32.exe" : L"Omnikey32.exe";

	hDlg = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG_UPDATER), 0, MainDialogProcess, 0);
	ShowWindow(hDlg, SW_SHOWNORMAL);
 
	MSG msg;
	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0)) {
		if (!IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK MainDialogProcess(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
	switch (message) {
	case WM_INITDIALOG:{
		HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_OMNIKEYUPDATE));
		if (hIcon) {
			SendMessage(hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
		}
		StartUpdate();
		return (INT_PTR)TRUE;
	}
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

DWORD WINAPI UpdateThreadFunction(LPVOID lpParam) {
	WCHAR path[MAX_PATH];
	WCHAR currentDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, currentDir);
	wsprintf(path, TEXT("%s\\_Omnikey.tempf"), currentDir);
	HRESULT res = URLDownloadToFile(NULL, L"https://raw.githubusercontent.com/jetaudio/Omnikey/master/version.json", path, 0, NULL);

	wstring data;
	if (res == S_OK) {
		std::wifstream t(path);
		std::wstringstream buffer;
		buffer << t.rdbuf();
		t.close();
		DeleteFile(path);
		data = buffer.str();
	} else {
		MessageBox(hDlg, _T("Có lỗi trong quá trình cập nhật, vui lòng thử lại sau!"), _T("Omnikey Update"), MB_OK);
		ExitProcess(0);
		return 0;
	}

	//simple parse
	data = data.substr(data.find(L"latestWinVersion"));
	data = data.substr(data.find(L"\"versionName\":"));
	data = data.substr(14);
	data = data.substr(data.find(L"\""));
	data = data.substr(1);
	wstring versionName = data.substr(0, data.find(L"\""));
	
	//download zip file
	WCHAR updateUrl[MAX_PATH];
	wsprintf(updateUrl, TEXT("https://github.com/jetaudio/Omnikey/releases/download/v%s/Omnikey-%s-Windows.zip"),
		versionName.c_str(),
		versionName.c_str());
	wsprintf(path, TEXT("%s\\_OmnikeyUpdate.zip"), currentDir);
	res = URLDownloadToFile(NULL, updateUrl, path, 0, NULL);

	if (res == S_OK) {
		BOOL host64 = FALSE;
#ifdef _WIN64
		host64 = TRUE;
#else
		IsWow64Process(GetCurrentProcess(), &host64);
#endif
		const wchar_t* mainExe = updateTarget ? updateTarget : (host64 && GetFileAttributesW(L"Omnikey64.exe") != INVALID_FILE_ATTRIBUTES
			? L"Omnikey64.exe" : L"Omnikey32.exe");
		// Wait for extraction/copy and keep the old EXE until the package is valid.
		// Relative literal paths avoid quoting user-controlled installation paths.
		HRSRC resource = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(201), RT_RCDATA);
		HGLOBAL loaded = resource ? LoadResource(GetModuleHandleW(nullptr), resource) : nullptr;
		const char* bytes = loaded ? static_cast<const char*>(LockResource(loaded)) : nullptr;
		DWORD byteCount = resource ? SizeofResource(GetModuleHandleW(nullptr), resource) : 0;
		if (!bytes || !byteCount) { ExitProcess(1); return 1; }
		int characters = MultiByteToWideChar(CP_UTF8, 0, bytes, byteCount, nullptr, 0);
		wstring script(characters, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, bytes, byteCount, &script[0], characters);
		script = L"& { " + script + L" } -MainExe '" + mainExe + L"'";
		DWORD encodedSize = 0;
		CryptBinaryToStringW(reinterpret_cast<const BYTE*>(script.data()), (DWORD)(script.size() * sizeof(wchar_t)), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &encodedSize);
		wstring encoded(encodedSize, L'\0');
		if (!CryptBinaryToStringW(reinterpret_cast<const BYTE*>(script.data()), (DWORD)(script.size() * sizeof(wchar_t)), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &encoded[0], &encodedSize)) { ExitProcess(1); return 1; }
		encoded.resize(encodedSize);
		wstring command = L"powershell.exe -NoProfile -NonInteractive -EncodedCommand " + encoded;
		STARTUPINFOW startup = { sizeof(startup) };
		PROCESS_INFORMATION process = {};
		DWORD result = 1;
		if (CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, currentDir, &startup, &process)) {
			WaitForSingleObject(process.hProcess, INFINITE);
			GetExitCodeProcess(process.hProcess, &result);
			CloseHandle(process.hThread);
			CloseHandle(process.hProcess);
		}
		if (result == 0) {
			DeleteFile(path);
			if (!RestartOmnikey(currentDir, mainExe)) {
				MessageBox(hDlg, _T("Đã cập nhật Omnikey thành công nhưng không thể tự khởi động lại. Hãy mở Omnikey từ thư mục cài đặt."), _T("Omnikey Update"), MB_OK | MB_ICONWARNING);
				ExitProcess(1);
			}
		} else {
			MessageBox(hDlg, _T("Không cập nhật được Omnikey. Hãy tải và giải nén đầy đủ gói Windows từ trang release."), _T("Omnikey Update"), MB_OK | MB_ICONERROR);
		}
		ExitProcess(0);
	} else {
		MessageBox(hDlg, _T("Có lỗi trong quá trình cập nhật, vui lòng thử lại sau!"), _T("Omnikey Update"), MB_OK);
		ExitProcess(0);
	}
	return 0;
}

void StartUpdate() {
	DWORD hThread;
	HANDLE t = CreateThread(
							NULL,                   // default security attributes
							0,                      // use default stack size  
							UpdateThreadFunction,       // thread function name
							0,          // argument to thread function 
							0,                      // use default creation flags 
							&hThread);   // returns the thread identifier 
}
