/*----------------------------------------------------------
Omnikey - The Cross platform Open source Vietnamese Keyboard application.

This file is belong to the Omnikey project, Win32 version
which is released under GPL license.
-----------------------------------------------------------*/
#pragma once
#include "stdafx.h"
#include <memory>

// Settings-style widgets shared by the Windows dialogs. They follow the macOS
// OKFormUI layout: grouped rows with the control on the trailing edge, switches
// instead of checkboxes, and light/dark colors taken from the system setting.
namespace FormUI {

struct Theme {
    bool dark;
    COLORREF window, group, separator, text, secondaryText, tertiaryText, accent,
             controlFill, controlBorder, controlPressed, track, trackOff, selection;
};

enum FontRole { FontBody, FontDetail, FontHeader, FontLargeTitle, FontToolbar, FontIcon, FontCount };
enum SegmentPosition { SegmentSingle, SegmentFirst, SegmentMiddle, SegmentLast };

void refreshTheme();
const Theme& theme();
int scale(int value);
HFONT font(FontRole role);

// Dark title bar, theme-aware native controls and a full repaint.
void applyWindowTheme(HWND window);
// WM_CTLCOLOR* answer for edits and combo lists.
INT_PTR controlColor(HDC hdc);
HBRUSH windowBrush();
void fillRect(HDC hdc, const RECT& rect, COLORREF color);
void drawFrame(HDC hdc, const RECT& rect);

HWND createToggle(HWND parent, int id, LPCWSTR name);
HWND createSegment(HWND parent, int id, LPCWSTR label, bool exclusive, SegmentPosition position);
HWND createButton(HWND parent, int id, LPCWSTR title, bool onGroup = true, bool isDefault = false);
HWND createToolbarItem(HWND parent, int id, LPCWSTR glyph, LPCWSTR label);
HWND createComboBox(HWND parent, int id, int width, const vector<LPCTSTR>& items);
HWND createKeyField(HWND parent, int id);
void setToolbarItemSelected(HWND item, bool selected);

// Restyles controls created from a dialog template.
void styleButton(HWND button, bool onGroup, bool isDefault = false);
void styleCheckBox(HWND checkbox);
void styleEdit(HWND edit, LPCWSTR placeholder);

SIZE preferredSize(HWND control);

struct Item {
    HWND control;
    LPCWSTR text;   // drawn label when control is NULL, e.g. "+"
    int gap;        // space before this item
};
inline Item item(HWND control, int gap = 0) { return { control, nullptr, gap }; }
inline Item textItem(LPCWSTR text, int gap = 8) { return { nullptr, text, gap }; }

struct Row {
    wstring title, detail;
    vector<Item> accessory;
};

struct About {
    HICON icon = nullptr;
    wstring name, tagline, version, body, footer;
    vector<HWND> buttons;
};

// A scrollable page of grouped sections, or a centered About page.
class Page {
public:
    Page(HWND parent);
    ~Page();
    HWND hwnd() const { return window; }

    void addSection(LPCWSTR header, const vector<Row>& rows, LPCWSTR note = nullptr);
    void setAbout(const About& value);
    void setActions(const vector<HWND>& buttons);

    // Height needed to show everything at the given width.
    int idealHeight(int width);
    void place(int x, int y, int width, int height);

private:
    struct Section { wstring header, note; vector<Row> rows; };
    struct Op { int kind; RECT rect; wstring text; FontRole font; COLORREF color; UINT format; };

    HWND window;
    vector<Section> sections;
    std::unique_ptr<About> about;
    vector<HWND> actions;
    vector<Op> ops;
    int contentHeight = 0;
    int scrollY = 0;
    bool scrollVisible = false;
    bool inLayout = false;

    int layout(int width, bool apply);
    int measure(HDC hdc, const wstring& text, FontRole role, int width, UINT format);
    void paint(HDC hdc);
    void relayout();
    void scrollTo(int y);
    void updateScrollBar();
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

}
