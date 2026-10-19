/*----------------------------------------------------------
Omnikey - The Cross platform Open source Vietnamese Keyboard application.

This file is belong to the Omnikey project, Win32 version
which is released under GPL license.
-----------------------------------------------------------*/
#include "FormUI.h"
#include <objidl.h> // GDI+ needs the COM types WIN32_LEAN_AND_MEAN leaves out
#include <gdiplus.h>
#include <dwmapi.h>
#include <Uxtheme.h>
#include <windowsx.h>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "comctl32.lib")

namespace FormUI {
namespace {

struct GdiplusRuntime {
    ULONG_PTR token = 0;
    GdiplusRuntime() { Gdiplus::GdiplusStartupInput input; Gdiplus::GdiplusStartup(&token, &input, nullptr); }
    ~GdiplusRuntime() { if (token) Gdiplus::GdiplusShutdown(token); }
};
void startGdiplus() { static GdiplusRuntime runtime; }

Theme current;
bool themeLoaded = false;
HBRUSH brushWindow = nullptr, brushField = nullptr;
HFONT fonts[FontCount] = {};
int dpi = 0;

enum Kind { KindToggle, KindSegment, KindButton, KindToolbar, KindCheck };
struct ControlInfo {
    Kind kind;
    SegmentPosition position = SegmentSingle;
    bool exclusive = false;
    bool onGroup = true;
    bool isDefault = false;
    bool selected = false;
    bool hover = false;
    wstring glyph;
    SIZE size = {};
};

const wchar_t pageClass[] = L"OmnikeyFormPage";

DWORD readDword(HKEY root, LPCWSTR path, LPCWSTR name, DWORD fallback) {
    DWORD value = 0, size = sizeof(value);
    if (RegGetValueW(root, path, name, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS) return value;
    return fallback;
}

COLORREF mix(COLORREF a, COLORREF b, float t) {
    return RGB((int)(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t),
               (int)(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t),
               (int)(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t));
}

Gdiplus::Color color(COLORREF value) {
    return Gdiplus::Color(255, GetRValue(value), GetGValue(value), GetBValue(value));
}

// Rounded rectangle; corners whose radius is 0 stay square (segment ends).
void roundedPath(Gdiplus::GraphicsPath& path, Gdiplus::RectF box, float leftRadius, float rightRadius) {
    float l = leftRadius * 2, r = rightRadius * 2;
    if (l > 0) path.AddArc(box.X, box.Y, l, l, 180, 90); else path.AddLine(box.X, box.Y, box.X, box.Y);
    if (r > 0) {
        path.AddArc(box.GetRight() - r, box.Y, r, r, 270, 90);
        path.AddArc(box.GetRight() - r, box.GetBottom() - r, r, r, 0, 90);
    } else {
        path.AddLine(box.GetRight(), box.Y, box.GetRight(), box.GetBottom());
    }
    if (l > 0) path.AddArc(box.X, box.GetBottom() - l, l, l, 90, 90); else path.AddLine(box.X, box.GetBottom(), box.X, box.GetBottom());
    path.CloseFigure();
}

Gdiplus::RectF rectF(const RECT& r, float inset = 0) {
    return Gdiplus::RectF(r.left + inset, r.top + inset, r.right - r.left - inset * 2, r.bottom - r.top - inset * 2);
}

bool fontInstalled(LPCWSTR face) {
    LOGFONTW query = {};
    query.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(query.lfFaceName, face, _TRUNCATE);
    bool found = false;
    HDC hdc = GetDC(nullptr);
    EnumFontFamiliesExW(hdc, &query, [](const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM found) -> int {
        *(bool*)found = true;
        return 0;
    }, (LPARAM)&found, 0);
    ReleaseDC(nullptr, hdc);
    return found;
}

int textWidth(LPCWSTR text, FontRole role) {
    HDC hdc = GetDC(nullptr);
    HGDIOBJ old = SelectObject(hdc, font(role));
    SIZE size = {};
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &size);
    SelectObject(hdc, old);
    ReleaseDC(nullptr, hdc);
    return size.cx;
}

void drawText(HDC hdc, const wstring& text, RECT rect, FontRole role, COLORREF textColor, UINT format) {
    HGDIOBJ old = SelectObject(hdc, font(role));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    DrawTextW(hdc, text.c_str(), -1, &rect, format | DT_NOPREFIX);
    SelectObject(hdc, old);
}

bool keyboardFocus(HWND hwnd) {
    return GetFocus() == hwnd && !(SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS);
}

void drawControl(HWND hwnd, HDC hdc, ControlInfo* info) {
    using namespace Gdiplus;
    const Theme& t = theme();
    RECT rc;
    GetClientRect(hwnd, &rc);
    COLORREF background = info->onGroup ? t.group : t.window;
    fillRect(hdc, rc, background);

    bool enabled = IsWindowEnabled(hwnd) != FALSE;
    LRESULT state = SendMessage(hwnd, BM_GETSTATE, 0, 0);
    bool pushed = (state & BST_PUSHED) != 0;
    bool checked = (state & BST_CHECKED) != 0;
    bool focus = keyboardFocus(hwnd);
    float s = scale(100) / 100.0f;
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 256);

    Graphics* g = new Graphics(hdc);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    Pen ring(color(t.accent), 2 * s);
    COLORREF label = enabled ? t.text : t.tertiaryText;
    RECT textRect = rc;
    FontRole textFont = FontBody;
    UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;

    switch (info->kind) {
    case KindToggle: {
        float w = 36 * s, h = 20 * s;
        RectF track((rc.right - w) / 2, (rc.bottom - h) / 2, w, h);
        COLORREF fill = checked ? t.accent : t.trackOff;
        if (!enabled) fill = mix(fill, background, 0.5f);
        GraphicsPath path; roundedPath(path, track, h / 2, h / 2);
        SolidBrush brush(color(fill));
        g->FillPath(&brush, &path);
        float pad = 2 * s, d = h - pad * 2;
        RectF knob(checked ? track.GetRight() - pad - d : track.X + pad, track.Y + pad, d, d);
        SolidBrush knobBrush(color(enabled ? RGB(255, 255, 255) : mix(RGB(255, 255, 255), background, 0.3f)));
        g->FillEllipse(&knobBrush, knob);
        Pen edge(Color(40, 0, 0, 0), 0.75f * s);
        g->DrawEllipse(&edge, knob);
        if (focus) {
            RectF outer = track; outer.Inflate(3 * s, 3 * s);
            GraphicsPath focusPath; roundedPath(focusPath, outer, outer.Height / 2, outer.Height / 2);
            g->DrawPath(&ring, &focusPath);
        }
        title[0] = 0;
        break;
    }
    case KindSegment: {
        float radius = 6 * s;
        bool left = info->position == SegmentSingle || info->position == SegmentFirst;
        bool right = info->position == SegmentSingle || info->position == SegmentLast;
        COLORREF trackColor = t.track;
        if (pushed) trackColor = mix(trackColor, t.text, 0.12f);
        else if (info->hover && enabled) trackColor = mix(trackColor, t.text, 0.06f);
        GraphicsPath path; roundedPath(path, rectF(rc), left ? radius : 0, right ? radius : 0);
        SolidBrush brush(color(trackColor));
        g->FillPath(&brush, &path);
        if (checked) {
            RectF inner = rectF(rc, 2 * s);
            GraphicsPath pill; roundedPath(pill, inner, radius - 1.5f * s, radius - 1.5f * s);
            SolidBrush pillBrush(color(info->exclusive ? t.controlFill : t.accent));
            g->FillPath(&pillBrush, &pill);
            if (info->exclusive) {
                Pen border(color(t.controlBorder), 1);
                g->DrawPath(&border, &pill);
            } else {
                label = RGB(255, 255, 255);
            }
        }
        if (focus) {
            RectF inner = rectF(rc, 1.5f * s);
            GraphicsPath focusPath; roundedPath(focusPath, inner, radius - 1, radius - 1);
            g->DrawPath(&ring, &focusPath);
        }
        break;
    }
    case KindButton: {
        float radius = 6 * s;
        RectF box = rectF(rc, 0.5f);
        GraphicsPath path; roundedPath(path, box, radius, radius);
        COLORREF fill;
        if (info->isDefault) {
            fill = pushed ? mix(t.accent, RGB(0, 0, 0), 0.18f) : (info->hover ? mix(t.accent, RGB(255, 255, 255), 0.08f) : t.accent);
            if (!enabled) fill = mix(fill, background, 0.5f);
            label = RGB(255, 255, 255);
        } else {
            fill = pushed ? t.controlPressed : (info->hover && enabled ? mix(t.controlFill, t.text, 0.04f) : t.controlFill);
        }
        SolidBrush brush(color(fill));
        g->FillPath(&brush, &path);
        if (!info->isDefault) {
            Pen border(color(t.controlBorder), 1);
            g->DrawPath(&border, &path);
        }
        if (focus) {
            RectF inner = rectF(rc, 1.5f * s);
            GraphicsPath focusPath; roundedPath(focusPath, inner, radius - 1, radius - 1);
            Pen focusPen(info->isDefault ? Color(255, 255, 255, 255) : color(t.accent), 1.5f * s);
            g->DrawPath(&focusPen, &focusPath);
        }
        break;
    }
    case KindToolbar: {
        if (info->selected || (info->hover && enabled)) {
            GraphicsPath path; roundedPath(path, rectF(rc, 1), 8 * s, 8 * s);
            SolidBrush brush(color(info->selected ? t.selection : mix(t.window, t.text, 0.04f)));
            g->FillPath(&brush, &path);
        }
        if (focus) {
            GraphicsPath path; roundedPath(path, rectF(rc, 1.5f * s), 7 * s, 7 * s);
            g->DrawPath(&ring, &path);
        }
        break;
    }
    case KindCheck: {
        float box = 16 * s;
        RectF square(1 * s, (rc.bottom - box) / 2, box, box);
        GraphicsPath path; roundedPath(path, square, 4 * s, 4 * s);
        SolidBrush brush(color(checked ? t.accent : t.controlFill));
        g->FillPath(&brush, &path);
        if (checked) {
            Pen mark(Color(255, 255, 255, 255), 1.8f * s);
            mark.SetLineJoin(LineJoinRound);
            mark.SetStartCap(LineCapRound);
            mark.SetEndCap(LineCapRound);
            PointF points[] = { PointF(square.X + box * 0.25f, square.Y + box * 0.52f),
                                PointF(square.X + box * 0.43f, square.Y + box * 0.7f),
                                PointF(square.X + box * 0.76f, square.Y + box * 0.32f) };
            g->DrawLines(&mark, points, 3);
        } else {
            Pen border(color(t.controlBorder), 1);
            g->DrawPath(&border, &path);
        }
        if (focus) {
            RectF outer = square; outer.Inflate(2 * s, 2 * s);
            GraphicsPath focusPath; roundedPath(focusPath, outer, 5 * s, 5 * s);
            g->DrawPath(&ring, &focusPath);
        }
        textRect.left = (LONG)(box + 9 * s);
        format = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
        break;
    }
    }
    delete g;

    if (info->kind == KindToolbar) {
        RECT glyph = rc, caption = rc;
        glyph.top = scale(6);
        glyph.bottom = scale(32);
        caption.top = scale(32);
        caption.bottom = rc.bottom - scale(4);
        drawText(hdc, info->glyph, glyph, FontIcon, info->selected ? t.accent : t.secondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        drawText(hdc, title, caption, FontToolbar, label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (title[0]) {
        drawText(hdc, title, textRect, textFont, label, format);
    }
}

LRESULT CALLBACK controlProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR ref) {
    ControlInfo* info = (ControlInfo*)ref;
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bitmap = CreateCompatibleBitmap(hdc, max(1L, rc.right), max(1L, rc.bottom));
        HGDIOBJ old = SelectObject(mem, bitmap);
        drawControl(hwnd, mem, info);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bitmap);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        drawControl(hwnd, (HDC)wParam, info);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        if (!info->hover) {
            info->hover = true;
            TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&track);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        info->hover = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_NCDESTROY: {
        RemoveWindowSubclass(hwnd, controlProc, 1);
        LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
        delete info;
        return result;
    }
    }
    LRESULT result = DefSubclassProc(hwnd, msg, wParam, lParam);
    switch (msg) {
    case BM_SETCHECK: case BM_SETSTATE: case BM_SETSTYLE: case BM_CLICK:
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: case WM_SETTEXT:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: case WM_KEYDOWN: case WM_KEYUP:
    case WM_UPDATEUISTATE: case WM_CAPTURECHANGED:
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    }
    return result;
}

ControlInfo* infoOf(HWND hwnd) {
    DWORD_PTR ref = 0;
    return GetWindowSubclass(hwnd, controlProc, 1, &ref) ? (ControlInfo*)ref : nullptr;
}

void attach(HWND hwnd, ControlInfo* info) {
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font(FontBody), FALSE);
    SetWindowSubclass(hwnd, controlProc, 1, (DWORD_PTR)info);
    SetWindowPos(hwnd, nullptr, 0, 0, info->size.cx, info->size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(hwnd, nullptr, FALSE);
}

HWND makeButton(HWND parent, int id, LPCWSTR text, DWORD style, ControlInfo* info) {
    HWND hwnd = CreateWindowExW(0, WC_BUTTONW, text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                                0, 0, info->size.cx, info->size.cy, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    attach(hwnd, info);
    return hwnd;
}

bool hasClass(HWND hwnd, LPCWSTR name) {
    wchar_t buffer[64];
    return GetClassNameW(hwnd, buffer, 64) && _wcsicmp(buffer, name) == 0;
}

void themeNative(HWND hwnd) {
    bool dark = theme().dark;
    if (hasClass(hwnd, WC_COMBOBOXW) || hasClass(hwnd, WC_EDITW)) {
        SetWindowTheme(hwnd, dark ? L"DarkMode_CFD" : nullptr, nullptr);
    } else if (hasClass(hwnd, pageClass)) {
        SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : nullptr, nullptr);
    } else if (hasClass(hwnd, WC_LISTVIEWW)) {
        const Theme& t = theme();
        SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        ListView_SetBkColor(hwnd, t.controlFill);
        ListView_SetTextBkColor(hwnd, t.controlFill);
        ListView_SetTextColor(hwnd, t.text);
    }
}

} // namespace

void refreshTheme() {
    startGdiplus();
    themeLoaded = true;
    bool dark = readDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", 1) == 0;
    DWORD accent = readDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", 0xFFD47800);
    Theme t;
    t.dark = dark;
    t.accent = accent & 0xFFFFFF;
    if (dark) {
        t.window = RGB(32, 32, 34);
        t.group = RGB(45, 45, 48);
        t.separator = RGB(62, 62, 66);
        t.text = RGB(236, 236, 240);
        t.secondaryText = RGB(160, 160, 166);
        t.tertiaryText = RGB(112, 112, 118);
        t.controlFill = RGB(70, 70, 74);
        t.controlBorder = RGB(88, 88, 92);
        t.controlPressed = RGB(92, 92, 96);
        t.track = RGB(58, 58, 62);
        t.trackOff = RGB(78, 78, 82);
        t.selection = RGB(58, 58, 62);
        t.accent = mix(t.accent, RGB(255, 255, 255), 0.18f);
    } else {
        t.window = RGB(246, 246, 248);
        t.group = RGB(235, 235, 238);
        t.separator = RGB(214, 214, 218);
        t.text = RGB(29, 29, 31);
        t.secondaryText = RGB(110, 110, 116);
        t.tertiaryText = RGB(160, 160, 166);
        t.controlFill = RGB(255, 255, 255);
        t.controlBorder = RGB(204, 204, 210);
        t.controlPressed = RGB(226, 226, 230);
        t.track = RGB(222, 222, 226);
        t.trackOff = RGB(204, 204, 210);
        t.selection = RGB(226, 226, 230);
    }
    current = t;
    if (brushWindow) DeleteObject(brushWindow);
    if (brushField) DeleteObject(brushField);
    brushWindow = CreateSolidBrush(t.window);
    brushField = CreateSolidBrush(t.controlFill);
}

const Theme& theme() {
    if (!themeLoaded) refreshTheme();
    return current;
}

int scale(int value) {
    if (dpi == 0) {
        HDC hdc = GetDC(nullptr);
        dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(nullptr, hdc);
    }
    return MulDiv(value, dpi, 96);
}

HFONT font(FontRole role) {
    if (!fonts[role]) {
        static const int sizes[FontCount] = { 14, 12, 14, 26, 12, 20 };
        static const int weights[FontCount] = { FW_NORMAL, FW_NORMAL, FW_SEMIBOLD, FW_BOLD, FW_NORMAL, FW_NORMAL };
        LPCWSTR face = L"Segoe UI";
        if (role == FontIcon) face = fontInstalled(L"Segoe Fluent Icons") ? L"Segoe Fluent Icons" : L"Segoe MDL2 Assets";
        fonts[role] = CreateFontW(-scale(sizes[role]), 0, 0, 0, weights[role], FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    }
    return fonts[role];
}

HBRUSH windowBrush() {
    theme();
    return brushWindow;
}

void fillRect(HDC hdc, const RECT& rect, COLORREF value) {
    HBRUSH brush = CreateSolidBrush(value);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

void drawFrame(HDC hdc, const RECT& rect) {
    startGdiplus();
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    float radius = (float)scale(6);
    roundedPath(path, rectF(rect, 0.5f), radius, radius);
    Gdiplus::Pen pen(color(theme().controlBorder), 1);
    g.DrawPath(&pen, &path);
}

void applyWindowTheme(HWND window) {
    BOOL dark = theme().dark;
    if (FAILED(DwmSetWindowAttribute(window, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
    EnumChildWindows(window, [](HWND child, LPARAM) -> BOOL { themeNative(child); return TRUE; }, 0);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
}

INT_PTR controlColor(HDC hdc) {
    const Theme& t = theme();
    SetTextColor(hdc, t.text);
    SetBkColor(hdc, t.controlFill);
    return (INT_PTR)brushField;
}

HWND createToggle(HWND parent, int id, LPCWSTR name) {
    ControlInfo* info = new ControlInfo{ KindToggle };
    info->size = { scale(46), scale(28) };
    return makeButton(parent, id, name, BS_AUTOCHECKBOX, info);
}

HWND createSegment(HWND parent, int id, LPCWSTR label, bool exclusive, SegmentPosition position) {
    ControlInfo* info = new ControlInfo{ KindSegment };
    info->exclusive = exclusive;
    info->position = position;
    info->size = { max(scale(40), textWidth(label, FontBody) + scale(24)), scale(28) };
    return makeButton(parent, id, label, exclusive ? BS_RADIOBUTTON : BS_AUTOCHECKBOX, info);
}

HWND createButton(HWND parent, int id, LPCWSTR title, bool onGroup, bool isDefault) {
    ControlInfo* info = new ControlInfo{ KindButton };
    info->onGroup = onGroup;
    info->isDefault = isDefault;
    info->size = { max(scale(80), textWidth(title, FontBody) + scale(32)), scale(30) };
    return makeButton(parent, id, title, isDefault ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, info);
}

HWND createToolbarItem(HWND parent, int id, LPCWSTR glyph, LPCWSTR label) {
    ControlInfo* info = new ControlInfo{ KindToolbar };
    info->onGroup = false;
    info->glyph = glyph;
    info->size = { max(scale(76), textWidth(label, FontToolbar) + scale(20)), scale(58) };
    return makeButton(parent, id, label, BS_PUSHBUTTON, info);
}

void setToolbarItemSelected(HWND item, bool selected) {
    if (ControlInfo* info = infoOf(item)) {
        info->selected = selected;
        InvalidateRect(item, nullptr, FALSE);
    }
}

HWND createComboBox(HWND parent, int id, int width, const vector<LPCTSTR>& items) {
    HWND hwnd = CreateWindowExW(0, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                0, 0, width, scale(300), parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessage(hwnd, WM_SETFONT, (WPARAM)font(FontBody), FALSE);
    for (LPCTSTR text : items) SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)text);
    themeNative(hwnd);
    return hwnd;
}

HWND createKeyField(HWND parent, int id) {
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_CENTER | ES_AUTOHSCROLL,
                                0, 0, scale(64), scale(28), parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    styleEdit(hwnd, L"Phím");
    return hwnd;
}

void styleButton(HWND button, bool onGroup, bool isDefault) {
    ControlInfo* info = new ControlInfo{ KindButton };
    info->onGroup = onGroup;
    info->isDefault = isDefault;
    RECT rc;
    GetWindowRect(button, &rc);
    info->size = { rc.right - rc.left, rc.bottom - rc.top };
    attach(button, info);
}

void styleCheckBox(HWND checkbox) {
    ControlInfo* info = new ControlInfo{ KindCheck };
    info->onGroup = false;
    RECT rc;
    GetWindowRect(checkbox, &rc);
    info->size = { rc.right - rc.left, rc.bottom - rc.top };
    attach(checkbox, info);
}

void styleEdit(HWND edit, LPCWSTR placeholder) {
    SendMessage(edit, WM_SETFONT, (WPARAM)font(FontBody), FALSE);
    if (placeholder) SendMessage(edit, EM_SETCUEBANNER, TRUE, (LPARAM)placeholder);
    themeNative(edit);
}

SIZE preferredSize(HWND control) {
    if (ControlInfo* info = infoOf(control)) return info->size;
    RECT rc;
    GetWindowRect(control, &rc);
    return { rc.right - rc.left, rc.bottom - rc.top };
}

/*------------------------------ Page ------------------------------*/

enum OpKind { OpText, OpGroup, OpSeparator, OpIcon };

Page::Page(HWND parent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = pageClass;
        RegisterClassW(&wc);
        registered = true;
    }
    window = CreateWindowExW(WS_EX_CONTROLPARENT, pageClass, L"", WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL,
                             0, 0, 0, 0, parent, nullptr, GetModuleHandle(nullptr), this);
    ShowScrollBar(window, SB_VERT, FALSE);
    themeNative(window);
}

Page::~Page() {
    if (IsWindow(window)) {
        SetWindowLongPtr(window, GWLP_USERDATA, 0);
        DestroyWindow(window);
    }
}

void Page::addSection(LPCWSTR header, const vector<Row>& rows, LPCWSTR note) {
    sections.push_back({ header ? header : L"", note ? note : L"", rows });
}

void Page::setAbout(const About& value) {
    about.reset(new About(value));
}

void Page::setActions(const vector<HWND>& buttons) {
    actions = buttons;
}

int Page::measure(HDC hdc, const wstring& text, FontRole role, int width, UINT format) {
    if (text.empty()) return 0;
    HGDIOBJ old = SelectObject(hdc, font(role));
    RECT r = { 0, 0, width, 0 };
    DrawTextW(hdc, text.c_str(), -1, &r, format | DT_CALCRECT | DT_NOPREFIX);
    SelectObject(hdc, old);
    return r.bottom;
}

int Page::layout(int width, bool apply) {
    const Theme& t = theme();
    HDC hdc = GetDC(window);
    vector<Op> out;
    const UINT wrap = DT_WORDBREAK | DT_LEFT;
    const UINT centered = DT_WORDBREAK | DT_CENTER;
    int padding = scale(20), inset = scale(12);
    int x0 = padding, inner = width - padding * 2;
    int y = padding;
    auto move = [&](HWND control, int x, int top, SIZE size) {
        if (!apply) return;
        // A combo box's window height includes its drop-down list.
        int height = hasClass(control, WC_COMBOBOXW) ? scale(300) : size.cy;
        SetWindowPos(control, nullptr, x, top - scrollY, size.cx, height, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    auto text = [&](const wstring& value, RECT rect, FontRole role, COLORREF color, UINT format) {
        out.push_back({ OpText, rect, value, role, color, format });
    };

    if (about) {
        int center = width / 2;
        y = scale(28);
        int icon = scale(96);
        out.push_back({ OpIcon, { center - icon / 2, y, center + icon / 2, y + icon }, L"", FontBody, 0, 0 });
        y += icon + scale(12);
        struct Line { const wstring* value; FontRole role; COLORREF color; int maxWidth; int after; };
        Line lines[] = {
            { &about->name, FontLargeTitle, t.text, inner, scale(4) },
            { &about->tagline, FontBody, t.secondaryText, inner, scale(8) },
            { &about->version, FontDetail, t.tertiaryText, inner, scale(18) },
            { &about->body, FontBody, t.secondaryText, min(inner, scale(440)), scale(20) },
        };
        for (const Line& line : lines) {
            int h = measure(hdc, *line.value, line.role, line.maxWidth, centered);
            text(*line.value, { center - line.maxWidth / 2, y, center + line.maxWidth / 2, y + h }, line.role, line.color, centered);
            y += h + line.after;
        }
        int total = 0, height = 0, gap = scale(8);
        for (HWND button : about->buttons) {
            SIZE size = preferredSize(button);
            total += size.cx + (total ? gap : 0);
            height = max(height, (int)size.cy);
        }
        int x = center - total / 2;
        for (HWND button : about->buttons) {
            SIZE size = preferredSize(button);
            move(button, x, y, size);
            x += size.cx + gap;
        }
        y += height + scale(24);
        int h = measure(hdc, about->footer, FontDetail, inner, centered);
        text(about->footer, { x0, y, x0 + inner, y + h }, FontDetail, t.tertiaryText, centered);
        y += h + scale(24);
        ReleaseDC(window, hdc);
        if (apply) ops.swap(out);
        return y;
    }

    for (size_t i = 0; i < sections.size(); i++) {
        const Section& section = sections[i];
        if (i > 0) y += scale(24);
        if (!section.header.empty()) {
            int h = measure(hdc, section.header, FontHeader, inner - inset * 2, wrap);
            text(section.header, { x0 + inset, y, x0 + inner - inset, y + h }, FontHeader, t.text, wrap);
            y += h + scale(8);
        }
        int groupTop = y;
        size_t groupIndex = out.size();
        out.push_back({ OpGroup, {}, L"", FontBody, t.group, 0 });
        y += scale(4);
        for (size_t r = 0; r < section.rows.size(); r++) {
            const Row& row = section.rows[r];
            if (r > 0) {
                out.push_back({ OpSeparator, { x0 + inset, y, x0 + inner - inset, y + 1 }, L"", FontBody, t.separator, 0 });
                y += 1;
            }
            int accessoryWidth = 0, accessoryHeight = 0;
            for (const Item& item : row.accessory) {
                SIZE size = item.control ? preferredSize(item.control) : SIZE{ textWidth(item.text, FontBody), scale(18) };
                accessoryWidth += item.gap + size.cx;
                accessoryHeight = max(accessoryHeight, (int)size.cy);
            }
            int textWidthAvailable = inner - inset * 2 - (accessoryWidth ? accessoryWidth + scale(16) : 0);
            int titleHeight = measure(hdc, row.title, FontBody, textWidthAvailable, wrap);
            int detailHeight = measure(hdc, row.detail, FontDetail, textWidthAvailable, wrap);
            int textHeight = titleHeight + (detailHeight ? scale(2) + detailHeight : 0);
            int rowHeight = max(scale(44), max(textHeight + scale(20), accessoryHeight + scale(12)));
            int top = y + (rowHeight - textHeight) / 2;
            text(row.title, { x0 + inset, top, x0 + inset + textWidthAvailable, top + titleHeight }, FontBody, t.text, wrap);
            if (detailHeight) {
                top += titleHeight + scale(2);
                text(row.detail, { x0 + inset, top, x0 + inset + textWidthAvailable, top + detailHeight }, FontDetail, t.secondaryText, wrap);
            }
            int x = x0 + inner - inset - accessoryWidth;
            for (const Item& item : row.accessory) {
                x += item.gap;
                if (item.control) {
                    SIZE size = preferredSize(item.control);
                    move(item.control, x, y + (rowHeight - size.cy) / 2, size);
                    x += size.cx;
                } else {
                    int w = textWidth(item.text, FontBody);
                    text(item.text, { x, y, x + w, y + rowHeight }, FontBody, t.secondaryText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    x += w;
                }
            }
            y += rowHeight;
        }
        y += scale(4);
        out[groupIndex].rect = { x0, groupTop, x0 + inner, y };
        if (!section.note.empty()) {
            y += scale(8);
            int h = measure(hdc, section.note, FontDetail, inner - inset * 2, wrap);
            text(section.note, { x0 + inset, y, x0 + inner - inset, y + h }, FontDetail, t.secondaryText, wrap);
            y += h;
        }
    }
    if (!actions.empty()) {
        y += scale(20);
        int total = 0, height = 0, gap = scale(10);
        for (HWND button : actions) {
            SIZE size = preferredSize(button);
            total += size.cx + (total ? gap : 0);
            height = max(height, (int)size.cy);
        }
        int x = x0 + inner - total;
        for (HWND button : actions) {
            SIZE size = preferredSize(button);
            move(button, x, y, size);
            x += size.cx + gap;
        }
        y += height;
    }
    y += padding;
    ReleaseDC(window, hdc);
    if (apply) ops.swap(out);
    return y;
}

int Page::idealHeight(int width) {
    return layout(width, false);
}

void Page::place(int x, int y, int width, int height) {
    SetWindowPos(window, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    relayout();
}

void Page::relayout() {
    if (inLayout) return;
    inLayout = true;
    RECT rc;
    GetClientRect(window, &rc);
    contentHeight = layout(rc.right, false);
    bool needScroll = contentHeight > rc.bottom;
    if (needScroll != scrollVisible) {
        scrollVisible = needScroll;
        ShowScrollBar(window, SB_VERT, needScroll);
        GetClientRect(window, &rc);
        contentHeight = layout(rc.right, false);
    }
    scrollY = max(0, min(scrollY, contentHeight - (int)rc.bottom));
    layout(rc.right, true);
    updateScrollBar();
    InvalidateRect(window, nullptr, FALSE);
    inLayout = false;
}

void Page::updateScrollBar() {
    if (!scrollVisible) return;
    RECT rc;
    GetClientRect(window, &rc);
    SCROLLINFO info = { sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS };
    info.nMax = contentHeight - 1;
    info.nPage = rc.bottom;
    info.nPos = scrollY;
    SetScrollInfo(window, SB_VERT, &info, TRUE);
}

void Page::scrollTo(int value) {
    RECT rc;
    GetClientRect(window, &rc);
    value = max(0, min(value, contentHeight - (int)rc.bottom));
    if (value == scrollY) return;
    scrollY = value;
    layout(rc.right, true);
    updateScrollBar();
    InvalidateRect(window, nullptr, FALSE);
    UpdateWindow(window);
}

void Page::paint(HDC target) {
    RECT rc;
    GetClientRect(window, &rc);
    HDC hdc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, max(1L, rc.right), max(1L, rc.bottom));
    HGDIOBJ old = SelectObject(hdc, bitmap);
    fillRect(hdc, rc, theme().window);
    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        float radius = (float)scale(12);
        for (const Op& op : ops) {
            if (op.kind != OpGroup) continue;
            RECT r = op.rect;
            OffsetRect(&r, 0, -scrollY);
            Gdiplus::GraphicsPath path;
            roundedPath(path, rectF(r), radius, radius);
            Gdiplus::SolidBrush brush(color(op.color));
            g.FillPath(&brush, &path);
        }
    }
    for (const Op& op : ops) {
        RECT r = op.rect;
        OffsetRect(&r, 0, -scrollY);
        if (op.kind == OpText) drawText(hdc, op.text, r, op.font, op.color, op.format);
        else if (op.kind == OpSeparator) fillRect(hdc, r, op.color);
        else if (op.kind == OpIcon && about && about->icon)
            DrawIconEx(hdc, r.left, r.top, about->icon, r.right - r.left, r.bottom - r.top, 0, nullptr, DI_NORMAL);
    }
    BitBlt(target, 0, 0, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, old);
    DeleteObject(bitmap);
    DeleteDC(hdc);
}

LRESULT CALLBACK Page::proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT*)lParam)->lpCreateParams);
    }
    Page* page = (Page*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_COMMAND:
    case WM_NOTIFY:
        return SendMessage(GetParent(hwnd), msg, wParam, lParam);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
        return controlColor((HDC)wParam);
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (page) page->paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE:
        if (page) page->relayout();
        return 0;
    case WM_MOUSEWHEEL:
        if (page && page->scrollVisible) {
            page->scrollTo(page->scrollY - GET_WHEEL_DELTA_WPARAM(wParam) * scale(48) / WHEEL_DELTA);
            return 0;
        }
        break;
    case WM_VSCROLL:
        if (page) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            SCROLLINFO info = { sizeof(info), SIF_ALL };
            GetScrollInfo(hwnd, SB_VERT, &info);
            int y = page->scrollY;
            switch (LOWORD(wParam)) {
            case SB_LINEUP: y -= scale(40); break;
            case SB_LINEDOWN: y += scale(40); break;
            case SB_PAGEUP: y -= rc.bottom; break;
            case SB_PAGEDOWN: y += rc.bottom; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: y = info.nTrackPos; break;
            case SB_TOP: y = 0; break;
            case SB_BOTTOM: y = page->contentHeight; break;
            }
            page->scrollTo(y);
        }
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

}
