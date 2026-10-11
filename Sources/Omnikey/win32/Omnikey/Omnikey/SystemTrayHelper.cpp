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
#include "SystemTrayHelper.h"
#include "AppDelegate.h"

#define WM_TRAYMESSAGE (WM_USER + 1)
#define TRAY_ICONUID 100

#define POPUP_VIET_ON_OFF 900
#define POPUP_SPELLING 901
#define POPUP_SMART_SWITCH 902
#define POPUP_USE_MACRO 903
#define POPUP_CHINESE 904

#define POPUP_TELEX 910
#define POPUP_VNI 911
#define POPUP_SIMPLE_TELEX_1 912
#define POPUP_SIMPLE_TELEX_2 913

#define POPUP_UNICODE 930
#define POPUP_TCVN3 931
#define POPUP_VNI_WINDOWS 932
#define POPUP_UNICODE_COMPOUND 933
#define POPUP_VN_LOCALE_1258 934

#define POPUP_CONVERT_TOOL 980
#define POPUP_QUICK_CONVERT 981

#define POPUP_MACRO_TABLE 990

#define POPUP_CONTROL_PANEL 1000
#define POPUP_ABOUT_OMNIKEY 1010
#define POPUP_OMNIKEY_EXIT 2000

#define MODIFY_MENU(MENU, COMMAND, DATA) ModifyMenu(MENU, COMMAND, \
											MF_BYCOMMAND | (DATA ? MF_CHECKED : MF_UNCHECKED), \
											COMMAND, \
											menuData[COMMAND]);

static HMENU popupMenu;
//static HMENU menuInputType;
static HMENU otherCode;

static NOTIFYICONDATA nid;

map<UINT, LPCTSTR> menuData = {
	{POPUP_VIET_ON_OFF, _T("Bật Tiếng Việt")},
	{POPUP_CHINESE, _T("Gõ tiếng Trung (Pinyin)")},
	{POPUP_SPELLING, _T("Bật kiểm tra chính tả")},
	{POPUP_SMART_SWITCH, _T("Bật loại trừ ứng dụng thông minh")},
	{POPUP_USE_MACRO, _T("Bật gõ tắt")},
	{POPUP_TELEX, _T("Kiểu gõ Telex")},
	{POPUP_VNI, _T("Kiểu gõ VNI")},
	{POPUP_SIMPLE_TELEX_1, _T("Kiểu gõ Simple Telex 1")},
	{POPUP_SIMPLE_TELEX_2, _T("Kiểu gõ Simple Telex 2")},
	{POPUP_UNICODE, _T("Unicode dựng sẵn")},
	{POPUP_TCVN3, _T("TCVN3 (ABC)")},
	{POPUP_VNI_WINDOWS, _T("VNI Windows")},
	{POPUP_UNICODE_COMPOUND, _T("Unicode tổ hợp")},
	{POPUP_VN_LOCALE_1258, _T("Vietnamese locale CP 1258")},
	{POPUP_CONVERT_TOOL, _T("Công cụ chuyển mã...")},
	{POPUP_QUICK_CONVERT, _T("Chuyển mã nhanh")},
	{POPUP_MACRO_TABLE, _T("Cấu hình gõ tắt...")},
	{POPUP_CONTROL_PANEL, _T("Bảng điều khiển...")},
	{POPUP_ABOUT_OMNIKEY, _T("Giới thiệu Omnikey")},
	{POPUP_OMNIKEY_EXIT, _T("Thoát")},
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	static UINT taskbarCreated;
	static bool sessionNotificationRegistered = false;

	switch (message) {
	case WM_CREATE:
		taskbarCreated = RegisterWindowMessage(_T("TaskbarCreated"));
        sessionNotificationRegistered = WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_THIS_SESSION) != FALSE;
        if (!sessionNotificationRegistered) OutputDebugString(_T("Omnikey: session notifications unavailable.\n"));
        break;
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_UNLOCK) OmnikeyManager::reinitHooks();
        break;
    case WM_DESTROY:
        if (sessionNotificationRegistered) WTSUnRegisterSessionNotification(hWnd);
        sessionNotificationRegistered = false;
		break;
	case WM_USER+2019:
		AppDelegate::getInstance()->onControlPanel();
		break;
	case WM_TRAYMESSAGE: {
		if (lParam == WM_LBUTTONDBLCLK) {
			AppDelegate::getInstance()->onControlPanel();
		}
		if (lParam == WM_LBUTTONUP) {
			AppDelegate::getInstance()->onToggleVietnamese();
			SystemTrayHelper::updateData();
		} else if (lParam == WM_RBUTTONDOWN) {
			POINT curPoint;
			GetCursorPos(&curPoint);
			SetForegroundWindow(hWnd);
			UINT commandId = TrackPopupMenu(
				popupMenu,
				TPM_RETURNCMD | TPM_NONOTIFY,
				curPoint.x,
				curPoint.y,
				0,
				hWnd,
				NULL
			);
			switch (commandId) {
			case POPUP_VIET_ON_OFF:
				AppDelegate::getInstance()->onToggleVietnamese();
				break;
			case POPUP_CHINESE:
				AppDelegate::getInstance()->selectInputMode(vChineseMode ? 1 : 2);
				break;
			case POPUP_SPELLING:
				AppDelegate::getInstance()->onToggleCheckSpelling();
				break;
			case POPUP_SMART_SWITCH:
				AppDelegate::getInstance()->onToggleUseSmartSwitchKey();
				break;
			case POPUP_USE_MACRO:
				AppDelegate::getInstance()->onToggleUseMacro();
				break;
			case POPUP_MACRO_TABLE:
				AppDelegate::getInstance()->onMacroTable();
				break;
			case POPUP_CONVERT_TOOL:
				AppDelegate::getInstance()->onConvertTool();
				break;
			case POPUP_QUICK_CONVERT:
				AppDelegate::getInstance()->onQuickConvert();
				break;
			case POPUP_TELEX:
				AppDelegate::getInstance()->onInputType(0);
				break;
			case POPUP_VNI:
				AppDelegate::getInstance()->onInputType(1);
				break;
			case POPUP_SIMPLE_TELEX_1:
				AppDelegate::getInstance()->onInputType(2);
				break;
			case POPUP_SIMPLE_TELEX_2:
				AppDelegate::getInstance()->onInputType(3);
				break;
			case POPUP_UNICODE:
				AppDelegate::getInstance()->onTableCode(0);
				break;
			case POPUP_TCVN3:
				AppDelegate::getInstance()->onTableCode(1);
				break;
			case POPUP_VNI_WINDOWS:
				AppDelegate::getInstance()->onTableCode(2);
				break;
			case POPUP_UNICODE_COMPOUND:
				AppDelegate::getInstance()->onTableCode(3);
				break;
			case POPUP_VN_LOCALE_1258:
				AppDelegate::getInstance()->onTableCode(4);
				break;
			case POPUP_CONTROL_PANEL:
				AppDelegate::getInstance()->onControlPanel();
				break;
			case POPUP_ABOUT_OMNIKEY:
				AppDelegate::getInstance()->onOmnikeyAbout();
				break;
			case POPUP_OMNIKEY_EXIT:
				AppDelegate::getInstance()->onOmnikeyExit();
				break;
			}
			SystemTrayHelper::updateData();
		}
	}
	break;
	default:
		// if the taskbar is restarted, add the system tray icon again
		if (message == taskbarCreated) {
			Shell_NotifyIcon(NIM_ADD, &nid);
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

HWND SystemTrayHelper::createFakeWindow(const HINSTANCE & hIns) {
	//create fake window
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = 0;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hIns;
	wcex.hIcon = LoadIcon(hIns, MAKEINTRESOURCE(IDI_APP_ICON));
	wcex.hCursor = NULL;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = APP_CLASS;
	wcex.hIconSm = NULL;
	ATOM atom = RegisterClassExW(&wcex);
	HWND hWnd = CreateWindowW(APP_CLASS, _T(""), WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hIns, nullptr);
	if (!hWnd) {
		return NULL;
	}
	ShowWindow(hWnd, 0);
	UpdateWindow(hWnd);
	return hWnd;
}

void SystemTrayHelper::createPopupMenu() {
	popupMenu = CreatePopupMenu();
	AppendMenu(popupMenu, MF_CHECKED, POPUP_VIET_ON_OFF, menuData[POPUP_VIET_ON_OFF]);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_CHINESE, menuData[POPUP_CHINESE]);
	AppendMenu(popupMenu, MF_SEPARATOR, 0, 0);
	AppendMenu(popupMenu, MF_CHECKED, POPUP_SPELLING, menuData[POPUP_SPELLING]);
	AppendMenu(popupMenu, MF_CHECKED, POPUP_SMART_SWITCH, menuData[POPUP_SMART_SWITCH]);
	AppendMenu(popupMenu, MF_CHECKED, POPUP_USE_MACRO, menuData[POPUP_USE_MACRO]);
	AppendMenu(popupMenu, MF_SEPARATOR, 0, 0);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_MACRO_TABLE, menuData[POPUP_MACRO_TABLE]);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_CONVERT_TOOL, menuData[POPUP_CONVERT_TOOL]);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_QUICK_CONVERT, menuData[POPUP_QUICK_CONVERT]);
	AppendMenu(popupMenu, MF_SEPARATOR, 0, 0);

	//menuInputType = CreatePopupMenu();
	AppendMenu(popupMenu, MF_CHECKED, POPUP_TELEX, menuData[POPUP_TELEX]);
	AppendMenu(popupMenu, MF_CHECKED, POPUP_VNI, menuData[POPUP_VNI]);
	AppendMenu(popupMenu, MF_CHECKED, POPUP_SIMPLE_TELEX_1, menuData[POPUP_SIMPLE_TELEX_1]);
	AppendMenu(popupMenu, MF_CHECKED, POPUP_SIMPLE_TELEX_2, menuData[POPUP_SIMPLE_TELEX_2]);

	//AppendMenu(popupMenu, MF_POPUP, (UINT_PTR)menuInputType, _T("Kiểu gõ"));
	AppendMenu(popupMenu, MF_SEPARATOR, 0, 0);

	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_UNICODE, menuData[POPUP_UNICODE]);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_TCVN3, menuData[POPUP_TCVN3]);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_VNI_WINDOWS, menuData[POPUP_VNI_WINDOWS]);

	otherCode = CreatePopupMenu();
	AppendMenu(otherCode, MF_CHECKED, POPUP_UNICODE_COMPOUND, menuData[POPUP_UNICODE_COMPOUND]);
	AppendMenu(otherCode, MF_CHECKED, POPUP_VN_LOCALE_1258, menuData[POPUP_VN_LOCALE_1258]);
	AppendMenu(popupMenu, MF_POPUP, (UINT_PTR)otherCode, _T("Bảng mã khác"));

	AppendMenu(popupMenu, MF_SEPARATOR, 0, 0);

	AppendMenu(popupMenu, MF_STRING, POPUP_CONTROL_PANEL, menuData[POPUP_CONTROL_PANEL]);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_ABOUT_OMNIKEY, menuData[POPUP_ABOUT_OMNIKEY]);
	AppendMenu(popupMenu, MF_SEPARATOR, 0, 0);
	AppendMenu(popupMenu, MF_UNCHECKED, POPUP_OMNIKEY_EXIT, menuData[POPUP_OMNIKEY_EXIT]);

	SetMenuDefaultItem(popupMenu, POPUP_CONTROL_PANEL, false);
}

static HICON chineseIcon(bool gray) {
	static HICON icons[2] = {};
	int index = gray ? 1 : 0;
	if (icons[index]) return icons[index];
	BITMAPINFO bitmap = {};
	bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap.bmiHeader.biWidth = 32;
	bitmap.bmiHeader.biHeight = -32;
	bitmap.bmiHeader.biPlanes = 1;
	bitmap.bmiHeader.biBitCount = 32;
	void* pixels = nullptr;
	HDC dc = CreateCompatibleDC(nullptr);
	HBITMAP color = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
	if (!color) { DeleteDC(dc); return LoadIcon(nullptr, IDI_APPLICATION); }
	HGDIOBJ old = SelectObject(dc, color);
	RECT rect = { 0, 0, 32, 32 };
	HBRUSH brush = CreateSolidBrush(gray ? RGB(80, 80, 80) : RGB(27, 91, 175));
	FillRect(dc, &rect, brush); DeleteObject(brush);
	HFONT glyph = CreateFontW(-28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
	HGDIOBJ oldFont = SelectObject(dc, glyph);
	SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(255, 255, 255));
	DrawTextW(dc, L"中", 1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	SelectObject(dc, oldFont); DeleteObject(glyph);
	GdiFlush();
	for (int i = 0; i < 32 * 32; ++i) static_cast<DWORD*>(pixels)[i] |= 0xff000000;
	SelectObject(dc, old);
	BYTE maskBits[128] = {};
	HBITMAP mask = CreateBitmap(32, 32, 1, 1, maskBits);
	ICONINFO info = { TRUE, 0, 0, mask, color };
	icons[index] = CreateIconIndirect(&info);
	DeleteObject(mask); DeleteObject(color); DeleteDC(dc);
	return icons[index];
}

static void loadTrayIcon() {
	if (vChineseMode) {
		nid.hIcon = chineseIcon(vUseGrayIcon != 0);
		wcscpy_s(nid.szTip, L"Omnikey - Tiếng Trung (Pinyin)");
		return;
	}
	int icon = 0;
	if (vLanguage) {
		icon = vUseGrayIcon ? IDI_ICON_STATUS_VIET_10 : IDI_ICON_STATUS_VIET;
		LoadString(GetModuleHandle(0), IDS_TRAY_TITLE_2, nid.szTip, 128);
	}
	else {
		icon = vUseGrayIcon ? IDI_ICON_STATUS_ENG_10 : IDI_ICON_STATUS_ENG;
		LoadString(GetModuleHandle(0), IDS_TRAY_TITLE, nid.szTip, 128);
	}
	nid.hIcon = LoadIcon(GetModuleHandle(0), MAKEINTRESOURCE(icon));
}

void SystemTrayHelper::updateData() {
	loadTrayIcon();
	Shell_NotifyIcon(NIM_MODIFY, &nid);

	MODIFY_MENU(popupMenu, POPUP_VIET_ON_OFF, !vChineseMode && vLanguage);
	MODIFY_MENU(popupMenu, POPUP_CHINESE, vChineseMode);
	MODIFY_MENU(popupMenu, POPUP_SPELLING, vCheckSpelling);
	MODIFY_MENU(popupMenu, POPUP_SMART_SWITCH, vUseSmartSwitchKey);
	MODIFY_MENU(popupMenu, POPUP_USE_MACRO, vUseMacro);
	MODIFY_MENU(popupMenu, POPUP_TELEX, vInputType == 0);
	MODIFY_MENU(popupMenu, POPUP_VNI, vInputType == 1);
	MODIFY_MENU(popupMenu, POPUP_SIMPLE_TELEX_1, vInputType == 2);
	MODIFY_MENU(popupMenu, POPUP_SIMPLE_TELEX_2, vInputType == 3);
	MODIFY_MENU(popupMenu, POPUP_UNICODE, vCodeTable == 0);
	MODIFY_MENU(popupMenu, POPUP_TCVN3, vCodeTable == 1);
	MODIFY_MENU(popupMenu, POPUP_VNI_WINDOWS, vCodeTable == 2);
	MODIFY_MENU(otherCode, POPUP_UNICODE_COMPOUND, vCodeTable == 3);
	MODIFY_MENU(otherCode, POPUP_VN_LOCALE_1258, vCodeTable == 4);

	wstring hotkey = L"";
	bool hasAdd = false;
	if (convertToolHotKey & 0x100) {
		hotkey += L"Ctrl";
		hasAdd = true;
	}
	if (convertToolHotKey & 0x200) {
		if (hasAdd)
			hotkey += L" + ";
		hotkey += L"Alt";
		hasAdd = true;
	}
	if (convertToolHotKey & 0x400) {
		if (hasAdd)
			hotkey += L" + ";
		hotkey += L"Win";
		hasAdd = true;
	}
	if (convertToolHotKey & 0x800) {
		if (hasAdd)
			hotkey += L" + ";
		hotkey += L"Shift";
		hasAdd = true;
	}

	unsigned short k = ((convertToolHotKey >> 24) & 0xFF);
	if (k != 0xFE) {
		if (hasAdd)
			hotkey += L" + ";
		if (k == VK_SPACE)
			hotkey += L"Space";
		else
			hotkey += (wchar_t)k;
	}

	wstring hotKeyString = menuData[POPUP_QUICK_CONVERT];
	if (hasAdd) {
		hotKeyString += L" - [";
		hotKeyString += hotkey;
		hotKeyString += L"]";
	}
	ModifyMenu(popupMenu, POPUP_QUICK_CONVERT, MF_BYCOMMAND | MF_UNCHECKED, POPUP_QUICK_CONVERT, hotKeyString.c_str());
}

static HINSTANCE ins;
static int recreateCount = 0;

void SystemTrayHelper::_createSystemTrayIcon(const HINSTANCE& hIns) {
	HWND hWnd = createFakeWindow(ins);
	
	if (hWnd == NULL) { //Use timer to create
		if (recreateCount >= 5) {
			PostQuitMessage(0);
			return;
		}
		ins = hIns;
		SetTimer(NULL, 0, 1000 * 3, (TIMERPROC)&WaitToCreateFakeWindow);
		++recreateCount;
		return;
	}
	createPopupMenu();

	//create system tray
	nid.cbSize = sizeof(NOTIFYICONDATA);
	nid.hWnd = hWnd;
	nid.uID = TRAY_ICONUID;
	nid.uVersion = NOTIFYICON_VERSION;
	nid.uCallbackMessage = WM_TRAYMESSAGE;
	loadTrayIcon();
	LoadString(ins, IDS_APP_TITLE, nid.szTip, 128);
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;

	// Shell_NotifyIcon may fail if the system tray icon is not fully initialized
	const int maxRetries = 5;
	for (int attempt = 0; attempt < maxRetries; ++attempt) {
		if (Shell_NotifyIcon(NIM_ADD, &nid)) {
			break;
		}
		Sleep(1000);
	}
}


void CALLBACK SystemTrayHelper::WaitToCreateFakeWindow(HWND hwnd, UINT uMsg, UINT timerId, DWORD dwTime) {
	_createSystemTrayIcon(ins);
	KillTimer(0, timerId);
}

void SystemTrayHelper::createSystemTrayIcon(const HINSTANCE& hIns) {
	_createSystemTrayIcon(hIns);
}

void SystemTrayHelper::removeSystemTray() {
	Shell_NotifyIcon(NIM_DELETE, &nid);
}
