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
#include "OmnikeyHelper.h"
#include <stdarg.h>
#include <Urlmon.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "version.lib")
#pragma comment(lib, "Urlmon.lib")

static BYTE* _regData = 0;

static LPCTSTR sk = TEXT("SOFTWARE\\jetaudio\\Omnikey");
// Settings of the app before it was renamed from OpenKey; copied over once.
static LPCTSTR legacySk = TEXT("SOFTWARE\\TuyenMai\\OpenKey");
static HKEY hKey;
static LPCTSTR _runOnStartupKeyPath = _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
static TCHAR _executePath[MAX_PATH];
static bool _hasGetPath = false;

static DWORD _cacheProcessId = 0, _tempProcessId = 0;
static HWND _tempWnd;
static TCHAR _exePath[1024] = { 0 };
static LPCTSTR _exeName = _exePath;
static HANDLE _proc;
static string _exeNameUtf8 = "TheOmnikeyProject";
static string _unknownProgram = "UnknownProgram";

int CF_RTF = RegisterClipboardFormat(_T("Rich Text Format"));
int CF_HTML = RegisterClipboardFormat(_T("HTML Format"));
int CF_OMNIKEY = RegisterClipboardFormat(_T("Omnikey Format"));

void OmnikeyHelper::openKey() {
	LONG nError = RegOpenKeyEx(HKEY_CURRENT_USER, sk, NULL, KEY_ALL_ACCESS, &hKey);
	if (nError == ERROR_FILE_NOT_FOUND) 	{
		nError = RegCreateKeyEx(HKEY_CURRENT_USER, sk, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
		HKEY legacy;
		if (nError == ERROR_SUCCESS && RegOpenKeyEx(HKEY_CURRENT_USER, legacySk, NULL, KEY_READ, &legacy) == ERROR_SUCCESS) {
			RegCopyTree(legacy, NULL, hKey);
			RegCloseKey(legacy);
		}
	}
	if (nError) {
		LOG(L"result %d\n", nError);
	}
}

void OmnikeyHelper::setRegInt(LPCTSTR key, const int & val) {
	openKey();
	RegSetValueEx(hKey, key, 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
	RegCloseKey(hKey);
}

int OmnikeyHelper::getRegInt(LPCTSTR key, const int & defaultValue) {
	openKey();
	int val = defaultValue;
	DWORD size = sizeof(val);
	if (ERROR_SUCCESS != RegQueryValueEx(hKey, key, 0, 0, (LPBYTE)&val, &size)) {
		val = defaultValue;
	}
	RegCloseKey(hKey);
	return val;
}

void OmnikeyHelper::setRegBinary(LPCTSTR key, const BYTE * pData, const int & size) {
	openKey();
	RegSetValueEx(hKey, key, 0, REG_BINARY, pData, size);
	RegCloseKey(hKey);
}

BYTE * OmnikeyHelper::getRegBinary(LPCTSTR key, DWORD& outSize) {
	openKey();
	if (_regData) {
		delete[] _regData;
		_regData = NULL;
	}
	DWORD size = 0;
	RegQueryValueEx(hKey, key, 0, 0, 0, &size);
	_regData = new BYTE[size];
	if (ERROR_SUCCESS != RegQueryValueEx(hKey, key, 0, 0, _regData, &size)) {
		delete[] _regData;
		_regData = NULL;
	}
	outSize = size;
	RegCloseKey(hKey);
	return _regData;
}

void OmnikeyHelper::registerRunOnStartup(const int& val) {
	// Remove the entries created before the app was renamed from OpenKey.
	RegOpenKeyEx(HKEY_CURRENT_USER, _runOnStartupKeyPath, NULL, KEY_ALL_ACCESS, &hKey);
	RegDeleteValue(hKey, _T("OpenKey"));
	RegCloseKey(hKey);
	WinExec("schtasks /delete /tn OpenKey /f", SW_HIDE);
	if (val) {
		if (vRunAsAdmin) {
			// Wide command line with the path quoted inside /tr, so paths with
			// spaces, Vietnamese letters or more than MAX_PATH bytes work.
			wstring command = L"schtasks /create /sc onlogon /tn Omnikey /rl highest /tr \"\\\"" + getFullPath() + L"\\\"\" /f";
			STARTUPINFOW startup = { sizeof(startup) };
			PROCESS_INFORMATION process = {};
			if (CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
				CloseHandle(process.hThread);
				CloseHandle(process.hProcess);
			}
		} else {
			RegOpenKeyEx(HKEY_CURRENT_USER, _runOnStartupKeyPath, NULL, KEY_ALL_ACCESS, &hKey);
			wstring path = getFullPath();
			RegSetValueEx(hKey, _T("Omnikey"), 0, REG_SZ, (byte*)path.c_str(), ((DWORD)path.size() + 1) * sizeof(TCHAR));
			RegCloseKey(hKey);
		}
	} else {
		RegOpenKeyEx(HKEY_CURRENT_USER, _runOnStartupKeyPath, NULL, KEY_ALL_ACCESS, &hKey);
		RegDeleteValue(hKey, _T("Omnikey"));
		RegCloseKey(hKey);
		WinExec("schtasks /delete  /tn Omnikey /f", SW_HIDE);
	}
}

LPTSTR OmnikeyHelper::getExecutePath() {
	if (!_hasGetPath) {
		HMODULE hModule = GetModuleHandleW(NULL);
		GetModuleFileNameW(hModule, _executePath, MAX_PATH);
		_hasGetPath = true;
	}
	return _executePath;
}

string& OmnikeyHelper::getFrontMostAppExecuteName() {
	_tempWnd = GetForegroundWindow();
	GetWindowThreadProcessId(_tempWnd, &_tempProcessId);
	if (_tempProcessId == _cacheProcessId) {
		return _exeNameUtf8;
	}
	_cacheProcessId = _tempProcessId;
	// An elevated app cannot be opened: do not keep the previous app's path.
	_exePath[0] = 0;
	_proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, _tempProcessId);
	if (_proc) {
		GetProcessImageFileName(_proc, _exePath, 1024);
		CloseHandle(_proc);
	}
	
	if (wcscmp(_exePath, _T("")) == 0) {
		return _unknownProgram;
	}
	_exeName = _tcsrchr(_exePath, '\\') + 1;
	if (wcscmp(_exeName, _T("Omnikey64.exe")) == 0 ||
		wcscmp(_exeName, _T("Omnikey32.exe")) == 0 ||
		wcscmp(_exeName, _T("OpenKey64.exe")) == 0 || // installs updated from OpenKey keep the old name
		wcscmp(_exeName, _T("OpenKey32.exe")) == 0 ||
		wcscmp(_exeName, _T("explorer.exe")) == 0) {
		return _exeNameUtf8;
	}
	//convert straight into the cached name, no temporary string
	int nameLength = (int)lstrlen(_exeName);
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, _exeName, nameLength, NULL, 0, NULL, NULL);
	_exeNameUtf8.assign(size_needed, '\0');
	WideCharToMultiByte(CP_UTF8, 0, _exeName, nameLength, &_exeNameUtf8[0], size_needed, NULL, NULL);
	//LOG(L"%s\n", utf8ToWideString(_exeNameUtf8).c_str());
	return _exeNameUtf8;
}

string & OmnikeyHelper::getLastAppExecuteName() {
	if (!vUseSmartSwitchKey)
		return getFrontMostAppExecuteName();
	return _exeNameUtf8;
}

wstring OmnikeyHelper::getFullPath() {
	HMODULE hModule = GetModuleHandle(NULL);
	TCHAR path[MAX_PATH];
	GetModuleFileName(hModule, path, MAX_PATH);
	wstring rs(path);
	return rs;
}

bool OmnikeyHelper::launchUpdater() {
	wstring directory = getFullPath();
	directory = directory.substr(0, directory.find_last_of(L"\\/"));
#ifdef _WIN64
	const wchar_t* relative = L"\\Rime\\bin\\x64\\OmnikeyUpdate.exe";
	const wchar_t* parameters = L"--x64";
#else
	const wchar_t* relative = L"\\Rime\\bin\\x86\\OmnikeyUpdate.exe";
	const wchar_t* parameters = L"--x86";
#endif
	wstring updater = directory + relative;
	if (GetFileAttributesW(updater.c_str()) == INVALID_FILE_ATTRIBUTES)
		updater = directory + L"\\_OmnikeyUpdate" + relative;
	if (GetFileAttributesW(updater.c_str()) == INVALID_FILE_ATTRIBUTES)
		updater = directory + L"\\OmnikeyUpdate.exe";
	wchar_t temporaryDirectory[MAX_PATH], temporaryFile[MAX_PATH];
	if (!GetTempPathW(MAX_PATH, temporaryDirectory) || !GetTempFileNameW(temporaryDirectory, L"OKU", 0, temporaryFile)) return false;
	wstring runnable = wstring(temporaryFile) + L".exe";
	DeleteFileW(temporaryFile);
	// Running a temporary copy lets the update replace its own bundled helper.
	if (!CopyFileW(updater.c_str(), runnable.c_str(), TRUE)) return false;
	if ((INT_PTR)ShellExecuteW(nullptr, L"open", runnable.c_str(), parameters, directory.c_str(), SW_SHOWNORMAL) > 32) return true;
	DeleteFileW(runnable.c_str());
	return false;
}

wstring OmnikeyHelper::getClipboardText(const int& type) {
	// Try opening the clipboard
	if (!OpenClipboard(nullptr)) {
		return _T("");
	}

	// Get handle of clipboard object for ANSI text
	HANDLE hData = GetClipboardData(type);
	if (hData == nullptr) {
		return _T("");
	}

	// Lock the handle to get the actual text pointer
	wchar_t * pszText = static_cast<wchar_t*>(GlobalLock(hData));
	if (pszText == nullptr) {
		return _T("");
	}

	// Save text in a string class instance
	wstring text(pszText);
	
	// Release the lock
	GlobalUnlock(hData);

	// Release the clipboard
	CloseClipboard();
	
	return text;
}

void OmnikeyHelper::setClipboardText(LPCTSTR data, const int & len, const int& type) {
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
	memcpy(GlobalLock(hMem), data, len * sizeof(WCHAR));
	GlobalUnlock(hMem);
	if (!OpenClipboard(0)) {
		GlobalFree(hMem);
		return;
	}
	EmptyClipboard();
	if (!SetClipboardData(type, hMem)) GlobalFree(hMem);
	CloseClipboard();
}

// CF_HTML is a header of byte offsets followed by the HTML they point into.
// Only the HTML is converted, and the offsets are written again for its new
// length. False when the data does not have that layout.
static bool convertClipboardHtml(string& data) {
	auto findField = [](const string& header, const char* name, size_t& digits, size_t& width) {
		size_t at = header.find(name);
		if (at == string::npos) return false;
		digits = at + strlen(name);
		width = 0;
		while (digits + width < header.size() && header[digits + width] >= '0' && header[digits + width] <= '9') width++;
		return width > 0;
	};
	size_t digits = 0, width = 0;
	if (!findField(data, "StartHTML:", digits, width) || width > 18) return false;
	size_t start = (size_t)strtoull(data.substr(digits, width).c_str(), nullptr, 10);
	if (start <= digits + width || start > data.size()) return false;
	string header = data.substr(0, start);
	string html = convertUtil(data.substr(start));
	size_t fragmentStart = html.find("<!--StartFragment-->");
	size_t fragmentEnd = html.find("<!--EndFragment-->");
	if (fragmentStart == string::npos || fragmentEnd == string::npos) return false;
	auto setField = [&](const char* name, size_t value) {
		if (!findField(header, name, digits, width)) return false;
		string number = to_string(value);
		if (number.size() > width) return false;
		header.replace(digits, width, string(width - number.size(), '0') + number); // same width: the header keeps its length
		return true;
	};
	if (!setField("EndHTML:", start + html.size()) ||
		!setField("StartFragment:", start + fragmentStart + strlen("<!--StartFragment-->")) ||
		!setField("EndFragment:", start + fragmentEnd)) return false;
	data = header + html;
	return true;
}

bool OmnikeyHelper::quickConvert() {
	//read data from clipboard
	//support Unicode raw string, Rich Text Format and HTML

	if (!OpenClipboard(nullptr)) {
		return false;
	}

	string dataHTML, dataRTF;
	wstring dataUnicode;

	char* pHTML = 0, pRTF = 0;
	wchar_t* pUnicode = 0;

	//HTML
	HANDLE hData = GetClipboardData(CF_HTML);
	if (hData) {
		pHTML = static_cast<char*>(GlobalLock(hData));
		GlobalUnlock(hData);
	}
	if (pHTML) {
		dataHTML = pHTML;
		if (!convertClipboardHtml(dataHTML)) pHTML = 0; // put back the text only
	}

	//UNICODE
	hData = GetClipboardData(CF_UNICODETEXT);
	if (hData) {
		pUnicode = static_cast<wchar_t*>(GlobalLock(hData));
		GlobalUnlock(hData);
	}
	if (pUnicode) {
		dataUnicode = pUnicode;
		dataUnicode = utf8ToWideString(convertUtil(wideStringToUtf8(dataUnicode)));
	}

	// Nothing to convert (an image, files): leave the clipboard as it is.
	if (!pHTML && !pUnicode) {
		CloseClipboard();
		return false;
	}
	EmptyClipboard();

	// Put back only the formats that were there.
	if (pHTML) {
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (dataHTML.size() + 1) * sizeof(char));
		if (hMem) {
			memcpy(GlobalLock(hMem), dataHTML.c_str(), (dataHTML.size() + 1) * sizeof(char));
			GlobalUnlock(hMem);
			if (!SetClipboardData(CF_HTML, hMem)) GlobalFree(hMem);
		}
	}
	if (pUnicode) {
		HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (dataUnicode.size() + 1) * sizeof(wchar_t));
		if (hMem) {
			memcpy(GlobalLock(hMem), dataUnicode.c_str(), (dataUnicode.size() + 1) * sizeof(wchar_t));
			GlobalUnlock(hMem);
			if (!SetClipboardData(CF_UNICODETEXT, hMem)) GlobalFree(hMem);
		}
	}

	CloseClipboard();
	return true;
}

DWORD OmnikeyHelper::getVersionNumber() {
	// get the filename of the executable containing the version resource
	TCHAR szFilename[MAX_PATH + 1] = { 0 };
	if (GetModuleFileName(NULL, szFilename, MAX_PATH) == 0) {
		return 0;
	}

	// allocate a block of memory for the version info
	DWORD dummy;
	UINT dwSize = GetFileVersionInfoSize(szFilename, &dummy);
	if (dwSize == 0) {
		return 0;
	}
	std::vector<BYTE> data(dwSize);

	// load the version info
	if (!GetFileVersionInfo(szFilename, NULL, dwSize, &data[0])) {
		return 0;
	}

	LPBYTE lpBuffer = NULL;

	if (VerQueryValue(&data[0], _T("\\"), (VOID FAR * FAR*) & lpBuffer, &dwSize)) {
		if (dwSize) {
			VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
			if (verInfo->dwSignature == 0xfeef04bd) {
				return ((verInfo->dwFileVersionMS >> 16) & 0xffff) |
					(((verInfo->dwFileVersionMS >> 0) & 0xffff) << 8) |
					(((verInfo->dwFileVersionLS >> 16) & 0xffff) << 16);
			}
		}
	}

	return 0;
}

wstring OmnikeyHelper::getVersionString() {
	TCHAR versionBuffer[MAX_PATH];
	DWORD ver = getVersionNumber();
	wsprintfW(versionBuffer, _T("%d.%d.%d"), ver & 0xFF, (ver>>8) & 0xFF, (ver >> 16) & 0xFF);
	return wstring(versionBuffer);
}

wstring OmnikeyHelper::getContentOfUrl(LPCTSTR url){
	WCHAR path[MAX_PATH];
	GetTempPath2(MAX_PATH, path);
	wsprintf(path, TEXT("%s\\_Omnikey.tempf"), path);
	HRESULT res = URLDownloadToFile(NULL, url, path, 0, NULL);
	
	if (res == S_OK) {
		std::wifstream t(path);
		std::wstringstream buffer;
		buffer << t.rdbuf();
		t.close();
		DeleteFile(path);
		return buffer.str();
	} else if (res == E_OUTOFMEMORY) {
		
	} else if (res == INET_E_DOWNLOAD_FAILURE) {
		
	} else {
		
	}
	return L"";
}
