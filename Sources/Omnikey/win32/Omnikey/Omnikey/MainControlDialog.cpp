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
#include "MainControlDialog.h"
#include "AppDelegate.h"
#include <Shlobj.h>

#define IDC_TOOLBAR_FIRST       3000
#define IDC_LINK_HOME_PAGE      3100
#define IDC_LINK_RELEASES       3101
#define IDC_LINK_FEEDBACK       3102

#define SETTINGS_WIDTH          540
#define SETTINGS_MAX_HEIGHT     620

static Uint16 _lastKeyCode;

static const LPCWSTR pageTitles[] = { L"Chung", L"Bộ gõ", L"Gõ tắt", L"Hệ thống", L"Thông tin" };
// Segoe Fluent Icons / MDL2 Assets: Settings, Keyboard, Edit, Monitor, Info.
static const LPCWSTR pageGlyphs[] = { L"\xE713", L"\xE765", L"\xE70F", L"\xE7F4", L"\xE946" };

using FormUI::Row;
using FormUI::item;
using FormUI::textItem;

MainControlDialog::MainControlDialog(const HINSTANCE& hInstance, const int& resourceId)
    : BaseDialog(hInstance, resourceId) {
}

MainControlDialog::~MainControlDialog() {
}

static HWND toggle(HWND parent, int id) {
    return FormUI::createToggle(parent, id, L"");
}

void MainControlDialog::buildGeneralPage(FormUI::Page& page) {
    HWND parent = page.hwnd();
    checkVietnamese = FormUI::createSegment(parent, IDC_RADIO_METHOD_VIETNAMESE, L"Tiếng Việt", true, FormUI::SegmentFirst);
    checkEnglish = FormUI::createSegment(parent, IDC_RADIO_METHOD_ENGLISH, L"English", true, FormUI::SegmentMiddle);
    checkChinese = FormUI::createSegment(parent, IDC_RADIO_METHOD_CHINESE, L"中文", true, FormUI::SegmentLast);
    createToolTip(checkVietnamese, IDS_STRING_VIET);
    createToolTip(checkEnglish, IDS_STRING_ENG);
    createToolTip(checkChinese, _T("Gõ tiếng Trung bằng Pinyin"));

    comboBoxInputType = FormUI::createComboBox(parent, IDC_COMBO_INPUT_TYPE, FormUI::scale(150), OmnikeyManager::getInputType());
    comboBoxTableCode = FormUI::createComboBox(parent, IDC_COMBO_TABLE_CODE, FormUI::scale(200), OmnikeyManager::getTableCode());
    page.addSection(nullptr, {
        { L"Chế độ gõ", L"", { item(checkVietnamese), item(checkEnglish), item(checkChinese) } },
        { L"Kiểu gõ", L"", { item(comboBoxInputType) } },
        { L"Bảng mã", L"Thường dùng Unicode", { item(comboBoxTableCode) } },
    });

    checkCtrl = FormUI::createSegment(parent, IDC_CHECK_SWITCH_KEY_CTRL, L"Ctrl", false, FormUI::SegmentFirst);
    checkAlt = FormUI::createSegment(parent, IDC_CHECK_SWITCH_KEY_ALT, L"Alt", false, FormUI::SegmentMiddle);
    checkWin = FormUI::createSegment(parent, IDC_CHECK_SWITCH_KEY_WIN, L"Win", false, FormUI::SegmentMiddle);
    checkShift = FormUI::createSegment(parent, IDC_CHECK_SWITCH_KEY_SHIFT, L"Shift", false, FormUI::SegmentLast);
    textSwitchKey = FormUI::createKeyField(parent, IDC_SWITCH_KEY_KEY);
    checkBeep = toggle(parent, IDC_CHECK_SWITCH_KEY_BEEP);
    createToolTip(checkCtrl, IDS_STRING_CTRL);
    createToolTip(checkAlt, IDS_STRING_ALT);
    createToolTip(checkWin, IDS_STRING_WIN);
    createToolTip(checkShift, IDS_STRING_SHIFT);
    createToolTip(textSwitchKey, IDS_STRING_SWITCH_KEY);
    page.addSection(L"Phím chuyển chế độ", {
        { L"Phím tắt", L"Xoay vòng Tiếng Việt → English → 中文",
            { item(checkCtrl), item(checkAlt), item(checkWin), item(checkShift), textItem(L"+"), item(textSwitchKey, FormUI::scale(8)) } },
        { L"Kêu beep khi chuyển chế độ", L"Không áp dụng với chuyển chế độ thông minh", { item(checkBeep) } },
    });

    checkSmartSwitchKey = toggle(parent, IDC_CHECK_SMART_SWITCH_KEY);
    checkRememberTableCode = toggle(parent, IDC_CHECK_SMART_SWITCH_CODE);
    checkAllowOtherLanguages = toggle(parent, IDC_CHECK_OTHER_LANGUAGES);
    page.addSection(L"Chuyển chế độ thông minh", {
        { L"Chuyển chế độ thông minh", L"Tự ghi nhớ chế độ gõ theo từng ứng dụng", { item(checkSmartSwitchKey) } },
        { L"Tự ghi nhớ bảng mã theo ứng dụng", L"", { item(checkRememberTableCode) } },
        { L"Tắt tiếng Việt khi bộ gõ hệ thống khác tiếng Anh", L"", { item(checkAllowOtherLanguages) } },
    });
}

void MainControlDialog::buildTypingPage(FormUI::Page& page) {
    HWND parent = page.hwnd();
    checkSpelling = toggle(parent, IDC_CHECK_SPELLING);
    checkRestoreIfWrongSpelling = toggle(parent, IDC_CHECK_RESTORE_IF_WRONG_SPELLING);
    checkAllowZWJF = toggle(parent, IDC_CHECK_ALLOW_ZJWF);
    page.addSection(L"Chính tả", {
        { L"Kiểm tra chính tả", L"Hạn chế gõ sai từ tiếng Việt", { item(checkSpelling) } },
        { L"Tự khôi phục phím với từ sai", L"Từ không đúng chính tả tiếng Việt được trả lại các phím đã gõ", { item(checkRestoreIfWrongSpelling) } },
        { L"Cho phép “z w j f” làm phụ âm đầu", L"", { item(checkAllowZWJF) } },
    });

    checkModernOrthorgraphy = toggle(parent, IDC_CHECK_MODERN_ORTHORGRAPHY);
    checkCapsFirstChar = toggle(parent, IDC_CHECK_CAPS_FIRST_CHAR);
    page.addSection(L"Dấu và chữ hoa", {
        { L"Đặt dấu kiểu mới: oà, uý", L"Thay vì kiểu cũ òa, úy", { item(checkModernOrthorgraphy) } },
        { L"Viết hoa chữ cái đầu câu", L"Sau dấu chấm câu và khi xuống dòng", { item(checkCapsFirstChar) } },
    });

    checkTempOffSpelling = toggle(parent, IDC_CHECK_TEMP_OFF_SPELLING);
    checkTempOffOmnikey = toggle(parent, IDC_CHECK_TEMP_OFF_OPEN_KEY);
    page.addSection(L"Phím tạm tắt", {
        { L"Tạm tắt chính tả bằng phím Ctrl", L"Cho từ như Đắk Lắk, Krông…; tự bật lại ở từ tiếp theo", { item(checkTempOffSpelling) } },
        { L"Tạm tắt Omnikey bằng phím Alt", L"Tạm ngừng cho tới khi bạn gõ một từ mới", { item(checkTempOffOmnikey) } },
    });
}

void MainControlDialog::buildMacroPage(FormUI::Page& page) {
    HWND parent = page.hwnd();
    checkUseMacro = toggle(parent, IDC_CHECK_USE_MACRO);
    checkUseMacroInEnglish = toggle(parent, IDC_CHECK_USE_MACRO_IN_ENGLISH);
    checkMacroAutoCaps = toggle(parent, IDC_CHECK_AUTO_CAPS);
    HWND macroTable = FormUI::createButton(parent, IDC_BUTTON_MACRO_TABLE, L"Mở bảng…");
    page.addSection(L"Gõ tắt", {
        { L"Cho phép gõ tắt", L"Tiết kiệm thời gian gõ các cụm từ thường dùng", { item(checkUseMacro) } },
        { L"Gõ tắt cả khi tắt tiếng Việt", L"", { item(checkUseMacroInEnglish) } },
        { L"Tự động viết hoa theo phím tắt", L"", { item(checkMacroAutoCaps) } },
        { L"Bảng gõ tắt", L"Quản lý danh sách từ gõ tắt", { item(macroTable) } },
    });

    checkQuickTelex = toggle(parent, IDC_CHECK_QUICK_TELEX);
    checkQuickStartConsonant = toggle(parent, IDC_CHECK_QUICK_START_CONSONANT);
    checkQuickEndConsonant = toggle(parent, IDC_CHECK_QUICK_END_CONSONANT);
    page.addSection(L"Gõ nhanh", {
        { L"Gõ nhanh phụ âm", L"cc→ch, gg→gi, kk→kh, nn→ng, qq→qu, pp→ph, tt→th", { item(checkQuickTelex) } },
        { L"Gõ tắt phụ âm đầu", L"f→ph, j→gi, w→qu  ·  fải→phải, jảng→giảng", { item(checkQuickStartConsonant) } },
        { L"Gõ tắt phụ âm cuối", L"g→ng, h→nh, k→ch  ·  nhah→nhanh, bák→bách", { item(checkQuickEndConsonant) } },
    });
}

void MainControlDialog::buildSystemPage(FormUI::Page& page) {
    HWND parent = page.hwnd();
    checkRunWithWindows = toggle(parent, IDC_CHECK_RUN_WITH_WINDOWS);
    checkShowOnStartup = toggle(parent, IDC_CHECK_SHOW_ON_STARTUP);
    checkCheckNewVersion = toggle(parent, IDC_CHECK_CHECK_UPDATE);
    checkRunAsAdmin = toggle(parent, IDC_CHECK_RUN_AS_ADMIN);
    checkCreateDesktopShortcut = toggle(parent, IDC_CHECK_CHECK_CREATE_SHORTCUT);
    page.addSection(L"Khởi động", {
        { L"Khởi động cùng Windows", L"", { item(checkRunWithWindows) } },
        { L"Mở cửa sổ này khi khởi động", L"", { item(checkShowOnStartup) } },
        { L"Kiểm tra bản mới khi khởi động", L"", { item(checkCheckNewVersion) } },
        { L"Chạy với quyền Admin", L"Để gõ được trong các ứng dụng chạy bằng quyền Admin", { item(checkRunAsAdmin) } },
        { L"Tạo biểu tượng trên Desktop", L"", { item(checkCreateDesktopShortcut) } },
    });

    checkModernIcon = toggle(parent, IDC_CHECK_MODERN_ICON);
    page.addSection(L"Giao diện", {
        { L"Biểu tượng hiện đại trên khay hệ thống", L"Biểu tượng đơn sắc, hợp với giao diện tối", { item(checkModernIcon) } },
    });

    checkFixRecommendBrowser = toggle(parent, IDC_CHECK_FIX_RECOMMEND_BROWSER);
    checkFixChromium = toggle(parent, IDC_CHECK_FIX_CHROMIUM);
    checkUseClipboard = toggle(parent, IDC_CHECK_USE_CLIPBOARD);
    checkSupportMetroApp = toggle(parent, IDC_CHECK_SUPPORT_METRO_APP);
    page.addSection(L"Tương thích", {
        { L"Sửa lỗi gợi ý", L"Tránh lặp chữ trên thanh địa chỉ trình duyệt, Excel…", { item(checkFixRecommendBrowser) } },
        { L"Sửa lỗi trên Chromium (beta)", L"Chrome, Edge, Brave", { item(checkFixChromium) } },
        { L"Gửi chữ qua clipboard", L"Mặc định nên tắt; chỉ bật khi ứng dụng gõ bị lỗi", { item(checkUseClipboard) } },
        { L"Hỗ trợ ứng dụng Metro", L"Ứng dụng Store chạy trong ApplicationFrameHost", { item(checkSupportMetroApp) } },
    });

    hUpdateButton = FormUI::createButton(parent, IDC_BUTTON_CHECK_UPDATE, L"Kiểm tra bản mới…");
    HWND defaultButton = FormUI::createButton(parent, ID_BTN_DEFAULT, L"Khôi phục…");
    HWND exitButton = FormUI::createButton(parent, IDBUTTON_EXIT, L"Thoát");
    page.addSection(L"Omnikey", {
        { L"Cập nhật", L"", { item(hUpdateButton) } },
        { L"Khôi phục cài đặt mặc định", L"", { item(defaultButton) } },
        { L"Thoát Omnikey", L"Ngừng bộ gõ và ẩn biểu tượng trên khay hệ thống", { item(exitButton) } },
    });
}

void MainControlDialog::buildAboutPage(FormUI::Page& page) {
    HWND parent = page.hwnd();
    FormUI::About about;
    about.icon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON), IMAGE_ICON,
                                  FormUI::scale(96), FormUI::scale(96), LR_DEFAULTCOLOR);
    about.name = L"Omnikey";
    about.tagline = L"Bộ gõ Tiếng Việt đa nền tảng";
    wchar_t buffer[256];
    wsprintfW(buffer, _T("Phiên bản %s cho Windows · Cập nhật %s"), OmnikeyHelper::getVersionString().c_str(), _T(__DATE__));
    about.version = buffer;
    about.body = L"Omnikey là bộ gõ đa nền tảng mã nguồn mở, được phát triển và duy trì tại GitHub của Omnikey, "
                 L"tiếp nối công trình ban đầu của Mai Vũ Tuyên.";
    about.buttons = {
        FormUI::createButton(parent, IDC_LINK_HOME_PAGE, L"Trang chủ", false),
        FormUI::createButton(parent, IDC_LINK_RELEASES, L"Bản phát hành", false),
        FormUI::createButton(parent, IDC_LINK_FEEDBACK, L"Góp ý", false),
    };
    about.footer = L"Omnikey · mã nguồn mở theo GPL-3.0";
    page.setAbout(about);
}

void MainControlDialog::initDialog() {
    HINSTANCE hIns = GetModuleHandleW(NULL);
    //dialog icon
    SET_DIALOG_ICON(IDI_APP_ICON);

    //set title version
    TCHAR title[256];
    TCHAR titleBuffer[256];
    LoadString(hIns, IDS_MAIN_DIALOG_TITLE, title, 256);
    wsprintfW(titleBuffer, title, OmnikeyHelper::getVersionString().c_str());
    SetWindowText(hDlg, titleBuffer);

    //the dialog template only provides the window; every control is built below
    vector<HWND> templateControls;
    for (HWND child = GetWindow(hDlg, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        templateControls.push_back(child);
    for (HWND child : templateControls)
        DestroyWindow(child);

    //toolbar
    int x = 0;
    for (int i = 0; i < _countof(pageTitles); i++) {
        HWND button = FormUI::createToolbarItem(hDlg, IDC_TOOLBAR_FIRST + i, pageGlyphs[i], pageTitles[i]);
        toolbarItems.push_back(button);
        x += FormUI::preferredSize(button).cx;
    }
    int gap = FormUI::scale(4);
    int itemHeight = FormUI::preferredSize(toolbarItems[0]).cy;
    toolbarHeight = itemHeight + FormUI::scale(16);
    x = (FormUI::scale(SETTINGS_WIDTH) - x - gap * ((int)toolbarItems.size() - 1)) / 2;
    for (HWND button : toolbarItems) {
        SIZE size = FormUI::preferredSize(button);
        SetWindowPos(button, nullptr, x, FormUI::scale(8), size.cx, size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        x += size.cx + gap;
    }

    //pages
    void (MainControlDialog::*builders[])(FormUI::Page&) = {
        &MainControlDialog::buildGeneralPage, &MainControlDialog::buildTypingPage, &MainControlDialog::buildMacroPage,
        &MainControlDialog::buildSystemPage, &MainControlDialog::buildAboutPage,
    };
    for (auto builder : builders) {
        pages.emplace_back(new FormUI::Page(hDlg));
        (this->*builder)(*pages.back());
    }

    FormUI::applyWindowTheme(hDlg);
    fillData();
    showPage(0);

    //center on the work area once the final size is known
    RECT window, work;
    GetWindowRect(hDlg, &window);
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    SetWindowPos(hDlg, nullptr,
                 work.left + (work.right - work.left - (window.right - window.left)) / 2,
                 work.top + max(0L, (work.bottom - work.top - FormUI::scale(SETTINGS_MAX_HEIGHT)) / 3),
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainControlDialog::showPage(int index) {
    if (index < 0 || index >= (int)pages.size()) return;
    currentPage = index;
    for (int i = 0; i < (int)toolbarItems.size(); i++)
        FormUI::setToolbarItemSelected(toolbarItems[i], i == index);

    //size the window to fit the page, like the macOS preference window
    int width = FormUI::scale(SETTINGS_WIDTH);
    RECT work;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    int maxHeight = min(FormUI::scale(SETTINGS_MAX_HEIGHT), (int)(work.bottom - work.top) - toolbarHeight - FormUI::scale(80));
    int height = min(pages[index]->idealHeight(width), maxHeight);

    RECT frame = { 0, 0, width, toolbarHeight + height };
    AdjustWindowRectEx(&frame, (DWORD)GetWindowLongPtr(hDlg, GWL_STYLE), FALSE, (DWORD)GetWindowLongPtr(hDlg, GWL_EXSTYLE));
    SetWindowPos(hDlg, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    pages[index]->place(0, toolbarHeight, width, height);
    for (int i = 0; i < (int)pages.size(); i++)
        ShowWindow(pages[i]->hwnd(), i == index ? SW_SHOW : SW_HIDE);
}

INT_PTR MainControlDialog::eventProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_INITDIALOG:
        this->hDlg = hDlg;
        initDialog();
        return TRUE;
    case WM_CTLCOLORDLG:
        return (INT_PTR)FormUI::windowBrush();
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
        return FormUI::controlColor((HDC)wParam);
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hDlg, &ps);
        RECT rc;
        GetClientRect(hDlg, &rc);
        RECT bar = { 0, 0, rc.right, toolbarHeight };
        FormUI::fillRect(hdc, bar, FormUI::theme().window);
        RECT line = { 0, toolbarHeight - 1, rc.right, toolbarHeight };
        FormUI::fillRect(hdc, line, FormUI::theme().separator);
        EndPaint(hDlg, &ps);
        return TRUE;
    }
    case WM_SETTINGCHANGE:
        if (lParam && lstrcmpW((LPCWSTR)lParam, L"ImmersiveColorSet") == 0) {
            FormUI::refreshTheme();
            FormUI::applyWindowTheme(hDlg);
        }
        break;
    case WM_CLOSE:
        AppDelegate::getInstance()->closeDialog(this);
        return TRUE;
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId >= IDC_TOOLBAR_FIRST && wmId < IDC_TOOLBAR_FIRST + (int)toolbarItems.size()) {
            showPage(wmId - IDC_TOOLBAR_FIRST);
            break;
        }
        switch (wmId) {
        case IDCANCEL:
            AppDelegate::getInstance()->closeDialog(this);
            break;
        case IDBUTTON_EXIT:
            AppDelegate::getInstance()->onOmnikeyExit();
            break;
        case ID_BTN_DEFAULT: {
            int msgboxID = MessageBox(
                hDlg,
                _T("Bạn có chắc chắn muốn thiết lập lại cài đặt gốc?"),
                _T("Omnikey"),
                MB_ICONEXCLAMATION | MB_YESNO
            );
            if (msgboxID == IDYES) {
                AppDelegate::getInstance()->onDefaultConfig();
            }
            break;
        }
        case IDC_BUTTON_MACRO_TABLE:
            AppDelegate::getInstance()->onMacroTable();
            break;
        case IDC_BUTTON_CHECK_UPDATE:
            onUpdateButton();
            break;
        case IDC_LINK_HOME_PAGE:
            ShellExecute(NULL, _T("open"), _T("https://github.com/jetaudio/Omnikey"), NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDC_LINK_RELEASES:
            ShellExecute(NULL, _T("open"), _T("https://github.com/jetaudio/Omnikey/releases"), NULL, NULL, SW_SHOWNORMAL);
            break;
        case IDC_LINK_FEEDBACK:
            ShellExecute(NULL, _T("open"), _T("https://github.com/jetaudio/Omnikey/issues"), NULL, NULL, SW_SHOWNORMAL);
            break;
        default:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                this->onComboBoxSelected((HWND)lParam, LOWORD(wParam));
            }
            else if (HIWORD(wParam) == BN_CLICKED) {
                this->onCheckboxClicked((HWND)lParam);
            }
            else if (HIWORD(wParam) == EN_CHANGE) {
                _lastKeyCode = OmnikeyManager::_lastKeyCode;
                if (_lastKeyCode > 0) {
                    OmnikeyManager::_lastKeyCode = 0;
                    this->onCharacter((HWND)lParam, _lastKeyCode);
                }
            }
            break;
        }
        break;
    }
    }

    return FALSE;
}

static void setChecked(HWND control, bool checked) {
    SendMessage(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void MainControlDialog::fillData() {
    SendMessage(comboBoxInputType, CB_SETCURSEL, vInputType, 0);
    SendMessage(comboBoxTableCode, CB_SETCURSEL, vCodeTable, 0);

    setChecked(checkCtrl, HAS_CONTROL(vSwitchKeyStatus));
    setChecked(checkAlt, HAS_OPTION(vSwitchKeyStatus));
    setChecked(checkWin, HAS_COMMAND(vSwitchKeyStatus));
    setChecked(checkShift, HAS_SHIFT(vSwitchKeyStatus));
    setSwitchKeyText(textSwitchKey, (vSwitchKeyStatus >> 24) & 0xFF);
    setChecked(checkBeep, HAS_BEEP(vSwitchKeyStatus));

    setChecked(checkVietnamese, !vChineseMode && vLanguage);
    setChecked(checkEnglish, !vChineseMode && !vLanguage);
    setChecked(checkChinese, vChineseMode);

    setChecked(checkModernOrthorgraphy, vUseModernOrthography);
    setChecked(checkFixRecommendBrowser, vFixRecommendBrowser);
    setChecked(checkShowOnStartup, vShowOnStartUp);
    setChecked(checkRunWithWindows, vRunWithWindows);
    setChecked(checkSpelling, vCheckSpelling);
    setChecked(checkRestoreIfWrongSpelling, vRestoreIfWrongSpelling);
    setChecked(checkModernIcon, vUseGrayIcon);
    setChecked(checkAllowZWJF, vAllowConsonantZFWJ);
    setChecked(checkTempOffSpelling, vTempOffSpelling);
    setChecked(checkQuickStartConsonant, vQuickStartConsonant);
    setChecked(checkQuickEndConsonant, vQuickEndConsonant);
    setChecked(checkRememberTableCode, vRememberCode);
    setChecked(checkAllowOtherLanguages, vOtherLanguage);
    setChecked(checkTempOffOmnikey, vTempOffOmnikey);

    setChecked(checkSmartSwitchKey, vUseSmartSwitchKey);
    setChecked(checkCapsFirstChar, vUpperCaseFirstChar);
    setChecked(checkQuickTelex, vQuickTelex);
    setChecked(checkUseMacro, vUseMacro);
    setChecked(checkUseMacroInEnglish, vUseMacroInEnglishMode);

    setChecked(checkMacroAutoCaps, vAutoCapsMacro);
    setChecked(checkSupportMetroApp, vSupportMetroApp);
    setChecked(checkCreateDesktopShortcut, vCreateDesktopShortcut);
    setChecked(checkRunAsAdmin, vRunAsAdmin);
    setChecked(checkCheckNewVersion, vCheckNewVersion);
    setChecked(checkUseClipboard, !vSendKeyStepByStep);
    setChecked(checkFixChromium, vFixChromiumBrowser);

    EnableWindow(checkRestoreIfWrongSpelling, vCheckSpelling);
    EnableWindow(checkAllowZWJF, vCheckSpelling);
    EnableWindow(checkTempOffSpelling, vCheckSpelling);
    EnableWindow(checkFixChromium, vFixRecommendBrowser);
}

void MainControlDialog::setSwitchKey(const unsigned short& code) {
    vSwitchKeyStatus &= 0xFFFFFF00;
    vSwitchKeyStatus |= code;
    vSwitchKeyStatus &= 0x00FFFFFF;
    vSwitchKeyStatus |= ((unsigned int)code << 24);
    APP_SET_DATA(vSwitchKeyStatus, vSwitchKeyStatus);
}

void MainControlDialog::onComboBoxSelected(const HWND& hCombobox, const int& comboboxId) {
    if (hCombobox == comboBoxInputType) {
        APP_SET_DATA(vInputType, (int)SendMessage(hCombobox, CB_GETCURSEL, 0, 0));
    }
    else if (hCombobox == comboBoxTableCode) {
        APP_SET_DATA(vCodeTable, (int)SendMessage(hCombobox, CB_GETCURSEL, 0, 0));
        if (vRememberCode) {
            setAppInputMethodStatus(OmnikeyHelper::getFrontMostAppExecuteName(), vLanguage | (vCodeTable << 1));
            saveSmartSwitchKeyData();
        }
    }
    SystemTrayHelper::updateData();
}

void MainControlDialog::onCheckboxClicked(const HWND& hWnd) {
    int val = 0;
    if (hWnd == checkCtrl) {
        val = (int)SendMessage(checkCtrl, BM_GETCHECK, 0, 0);
        vSwitchKeyStatus &= (~0x100);
        vSwitchKeyStatus |= val << 8;
        APP_SET_DATA(vSwitchKeyStatus, vSwitchKeyStatus);
    }
    else if (hWnd == checkAlt) {
        val = (int)SendMessage(checkAlt, BM_GETCHECK, 0, 0);
        vSwitchKeyStatus &= (~0x200);
        vSwitchKeyStatus |= val << 9;
        APP_SET_DATA(vSwitchKeyStatus, vSwitchKeyStatus);
    }
    else if (hWnd == checkWin) {
        val = (int)SendMessage(checkWin, BM_GETCHECK, 0, 0);
        vSwitchKeyStatus &= (~0x400);
        vSwitchKeyStatus |= val << 10;
        APP_SET_DATA(vSwitchKeyStatus, vSwitchKeyStatus);
    }
    else if (hWnd == checkShift) {
        val = (int)SendMessage(checkShift, BM_GETCHECK, 0, 0);
        vSwitchKeyStatus &= (~0x800);
        vSwitchKeyStatus |= val << 11;
        APP_SET_DATA(vSwitchKeyStatus, vSwitchKeyStatus);
    }
    else if (hWnd == checkBeep) {
        val = (int)SendMessage(checkBeep, BM_GETCHECK, 0, 0);
        vSwitchKeyStatus &= (~0x8000);
        vSwitchKeyStatus |= val << 15;
        APP_SET_DATA(vSwitchKeyStatus, vSwitchKeyStatus);
    }
    //language segments are plain radio buttons; selectInputMode refreshes them through fillData
    else if (hWnd == checkVietnamese) {
        AppDelegate::getInstance()->selectInputMode(1);
    }
    else if (hWnd == checkEnglish) {
        AppDelegate::getInstance()->selectInputMode(0);
    }
    else if (hWnd == checkChinese) {
        AppDelegate::getInstance()->selectInputMode(2);
        fillData();
    }
    else if (hWnd == checkModernOrthorgraphy) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vUseModernOrthography, val ? 1 : 0);
    }
    else if (hWnd == checkFixRecommendBrowser) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vFixRecommendBrowser, val ? 1 : 0);
        EnableWindow(checkFixChromium, vFixRecommendBrowser);
    }
    else if (hWnd == checkShowOnStartup) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vShowOnStartUp, val ? 1 : 0);
    }
    else if (hWnd == checkRunWithWindows) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vRunWithWindows, val ? 1 : 0);
        OmnikeyHelper::registerRunOnStartup(vRunWithWindows);
    }
    else if (hWnd == checkSpelling) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vCheckSpelling, val ? 1 : 0);
        vSetCheckSpelling();
        EnableWindow(checkRestoreIfWrongSpelling, vCheckSpelling);
        EnableWindow(checkAllowZWJF, vCheckSpelling);
        EnableWindow(checkTempOffSpelling, vCheckSpelling);
    }
    else if (hWnd == checkRestoreIfWrongSpelling) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vRestoreIfWrongSpelling, val ? 1 : 0);
    }
    else if (hWnd == checkUseClipboard) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vSendKeyStepByStep, val ? 0 : 1);
    }
    else if (hWnd == checkSmartSwitchKey) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vUseSmartSwitchKey, val ? 1 : 0);
    }
    else if (hWnd == checkCapsFirstChar) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vUpperCaseFirstChar, val ? 1 : 0);
    }
    else if (hWnd == checkQuickTelex) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vQuickTelex, val ? 1 : 0);
    }
    else if (hWnd == checkUseMacro) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vUseMacro, val ? 1 : 0);
    }
    else if (hWnd == checkUseMacroInEnglish) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vUseMacroInEnglishMode, val ? 1 : 0);
    }
    else if (hWnd == checkModernIcon) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vUseGrayIcon, val ? 1 : 0);
    }
    else if (hWnd == checkAllowZWJF) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vAllowConsonantZFWJ, val ? 1 : 0);
    }
    else if (hWnd == checkTempOffSpelling) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vTempOffSpelling, val ? 1 : 0);
    }
    else if (hWnd == checkQuickStartConsonant) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vQuickStartConsonant, val ? 1 : 0);
    }
    else if (hWnd == checkQuickEndConsonant) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vQuickEndConsonant, val ? 1 : 0);
    }
    else if (hWnd == checkSupportMetroApp) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vSupportMetroApp, val ? 1 : 0);
    }
    else if (hWnd == checkMacroAutoCaps) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vAutoCapsMacro, val ? 1 : 0);
    }
    else if (hWnd == checkCreateDesktopShortcut) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vCreateDesktopShortcut, val ? 1 : 0);
        //create desktop shortcut
        if (val)
            OmnikeyManager::createDesktopShortcut();
    }
    else if (hWnd == checkRunAsAdmin) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vRunAsAdmin, val ? 1 : 0);
        requestRestartAsAdmin();
    }
    else if (hWnd == checkCheckNewVersion) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vCheckNewVersion, val ? 1 : 0);
    }
    else if (hWnd == checkRememberTableCode) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vRememberCode, val ? 1 : 0);
    }
    else if (hWnd == checkAllowOtherLanguages) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vOtherLanguage, val ? 1 : 0);
    }
    else if (hWnd == checkTempOffOmnikey) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vTempOffOmnikey, val ? 1 : 0);
    }
    else if (hWnd == checkFixChromium) {
        val = (int)SendMessage(hWnd, BM_GETCHECK, 0, 0);
        APP_SET_DATA(vFixChromiumBrowser, val ? 1 : 0);
    }
    SystemTrayHelper::updateData();
}

void MainControlDialog::onCharacter(const HWND& hWnd, const UINT16& keyCode) {
    if (keyCode == 0) return;
    if (hWnd == textSwitchKey) {
        UINT16 code = GET_SWITCH_KEY(vSwitchKeyStatus);
        if (keyCode == VK_DELETE || keyCode == VK_BACK) {
            code = 0xFE;
        }
        else if (keyCodeToCharacter(keyCode) != 0) {
            code = keyCode;
        }
        setSwitchKey(code);
        setSwitchKeyText(hWnd, code);
    }
}

void MainControlDialog::setSwitchKeyText(const HWND& hWnd, const UINT16& keyCode) {
    if (keyCode == KEY_SPACE) {
        SetWindowText(hWnd, _T("Space"));
    }
    else if (keyCode == 0xFE) {
        SetWindowText(hWnd, _T(""));
    }
    else {
        Uint16 key[] = { keyCode, 0 };
        SetWindowText(hWnd, (LPCWSTR)&key);
    }
}

void MainControlDialog::onUpdateButton() {
    EnableWindow(hUpdateButton, false);
    string newVersion;
    if (OmnikeyManager::checkUpdate(newVersion)) {
        WCHAR msg[256];
        wsprintf(msg,
            TEXT("Omnikey Có phiên bản mới (%s), bạn có muốn cập nhật không?"),
            utf8ToWideString(newVersion).c_str());

        int msgboxID = MessageBox(
            hDlg,
            msg,
            _T("Omnikey Update"),
            MB_ICONEXCLAMATION | MB_YESNO
        );
        if (msgboxID == IDYES) {
            if (OmnikeyHelper::launchUpdater()) AppDelegate::getInstance()->onOmnikeyExit();
            else MessageBoxW(hDlg, L"Không mở được trình cập nhật. Hãy tải gói Windows từ trang release.", L"Omnikey Update", MB_OK | MB_ICONERROR);
        }

    }
    else {
        MessageBox(hDlg, _T("Bạn đang dùng phiên bản mới nhất!"), _T("Omnikey Update"), MB_OK);
    }
    EnableWindow(hUpdateButton, true);
}

void MainControlDialog::requestRestartAsAdmin() {
    OmnikeyHelper::registerRunOnStartup(false);
    if (vRunAsAdmin && !IsUserAnAdmin()) {
        int msgboxID = MessageBox(
            hDlg,
            _T("Bạn cần phải khởi động lại Omnikey để kích hoạt chế độ Admin!\nBạn có muốn khởi động lại Omnikey không?"),
            _T("Omnikey"),
            MB_ICONEXCLAMATION | MB_YESNO
        );
        if (msgboxID == IDYES) {
            PostQuitMessage(0);
            ShellExecute(0, L"runas", OmnikeyHelper::getFullPath().c_str(), 0, 0, SW_SHOWNORMAL);
        }
    }
    else {
        OmnikeyHelper::registerRunOnStartup(vRunWithWindows);
    }
}
