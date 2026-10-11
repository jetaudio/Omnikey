#include "CandidatePanel.h"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#pragma comment(lib, "gdiplus.lib")

namespace CandidatePanel {
namespace {
    struct Runtime {
        ULONG_PTR token = 0;
        Runtime() { Gdiplus::GdiplusStartupInput input; Gdiplus::GdiplusStartup(&token, &input, nullptr); }
        ~Runtime() { if (token) Gdiplus::GdiplusShutdown(token); }
    };
    void initialize() { static Runtime runtime; }
    Gdiplus::RectF rect(const RECT& value) {
        return Gdiplus::RectF((float)value.left, (float)value.top, (float)(value.right - value.left), (float)(value.bottom - value.top));
    }
    void rounded(Gdiplus::GraphicsPath& path, Gdiplus::RectF box, float radius) {
        float diameter = radius * 2;
        path.AddArc(box.X, box.Y, diameter, diameter, 180, 90);
        path.AddArc(box.GetRight() - diameter, box.Y, diameter, diameter, 270, 90);
        path.AddArc(box.GetRight() - diameter, box.GetBottom() - diameter, diameter, diameter, 0, 90);
        path.AddArc(box.X, box.GetBottom() - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
    }
    float textWidth(Gdiplus::Graphics& graphics, const std::wstring& text, Gdiplus::Font& font) {
        Gdiplus::RectF bounds;
        graphics.MeasureString(text.c_str(), (INT)text.size(), &font, Gdiplus::PointF(0, 0), &bounds);
        return std::ceil(bounds.Width);
    }
    std::wstring label(const ChineseComposition& composition, size_t index) {
        return index < composition.selectLabels.size() ? composition.selectLabels.substr(index, 1) : std::to_wstring(index + 1);
    }
    void text(Gdiplus::Graphics& graphics, const std::wstring& value, Gdiplus::Font& font,
              Gdiplus::RectF box, Gdiplus::Color color) {
        Gdiplus::StringFormat format;
        format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::SolidBrush brush(color);
        graphics.DrawString(value.c_str(), (INT)value.size(), &font, box, &format, &brush);
    }
    void draw(Gdiplus::Graphics& graphics, const ChineseComposition& composition, const Layout& layout, bool dark, int hover) {
        using namespace Gdiplus;
        float s = layout.scale;
        graphics.SetSmoothingMode(SmoothingModeAntiAlias);
        graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        graphics.Clear(Color(0, 0, 0, 0));
        RectF surface = rect(layout.surface);
        // Soft alpha shadow, independent of OS version and without a window frame.
        for (int spread = 10; spread >= 1; --spread) {
            RectF shadow = surface;
            shadow.Inflate(spread * s * 0.7f, spread * s * 0.7f);
            shadow.Y += 3 * s;
            GraphicsPath path; rounded(path, shadow, (14 + spread * 0.5f) * s);
            SolidBrush brush(Color(dark ? 5 : 3, 0, 0, 0));
            graphics.FillPath(&brush, &path);
        }
        GraphicsPath card; rounded(card, surface, 14 * s);
        SolidBrush fill(dark ? Color(245, 35, 35, 39) : Color(246, 250, 250, 252));
        graphics.FillPath(&fill, &card);
        Pen outline(dark ? Color(65, 255, 255, 255) : Color(36, 80, 80, 95), s);
        graphics.DrawPath(&outline, &card);
        Font preeditFont(L"Segoe UI", 13 * s, FontStyleRegular, UnitPixel);
        Font numberFont(L"Segoe UI", 11 * s, FontStyleRegular, UnitPixel);
        Font candidateFont(L"Microsoft YaHei UI", 17 * s, FontStyleRegular, UnitPixel);
        Font commentFont(L"Segoe UI", 11 * s, FontStyleRegular, UnitPixel);
        Color secondary = dark ? Color(255, 165, 165, 174) : Color(255, 106, 106, 116);
        Color primary = dark ? Color(255, 245, 245, 248) : Color(255, 26, 26, 32);
        text(graphics, composition.preedit, preeditFont, rect(layout.preedit), secondary);
        for (size_t i = 0; i < layout.cells.size(); ++i) {
            RectF cell = rect(layout.cells[i]);
            bool selected = (int)i == composition.highlighted;
            if (selected || (int)i == hover) {
                GraphicsPath path; rounded(path, cell, 8 * s);
                SolidBrush highlight(selected ? Color(255, 0, 122, 255) : (dark ? Color(255, 62, 62, 68) : Color(255, 232, 235, 241)));
                graphics.FillPath(&highlight, &path);
            }
            std::wstring number = label(composition, i);
            float numberWidth = textWidth(graphics, number, numberFont);
            float x = cell.X + 8 * s;
            text(graphics, number, numberFont, RectF(x, cell.Y + 2 * s, numberWidth, cell.Height - 2 * s), selected ? Color(225, 255, 255, 255) : secondary);
            x += numberWidth + 4 * s;
            float candidateWidth = (std::min)(textWidth(graphics, composition.candidates[i], candidateFont), cell.GetRight() - x - 8 * s);
            text(graphics, composition.candidates[i], candidateFont, RectF(x, cell.Y, candidateWidth, cell.Height), selected ? Color(255, 255, 255, 255) : primary);
            x += candidateWidth + 4 * s;
            if (i < composition.comments.size() && !composition.comments[i].empty())
                text(graphics, composition.comments[i], commentFont, RectF(x, cell.Y + 2 * s, (std::max)(0.f, cell.GetRight() - x - 8 * s), cell.Height - 2 * s), selected ? Color(195, 255, 255, 255) : secondary);
        }
        auto chevron = [&](RECT bounds, bool previous, bool enabled, int id) {
            if (bounds.right == bounds.left) return;
            RectF box = rect(bounds);
            if (enabled && hover == id) {
                GraphicsPath path; rounded(path, box, 6 * s);
                SolidBrush fill(dark ? Color(255, 62, 62, 68) : Color(255, 232, 235, 241)); graphics.FillPath(&fill, &path);
            }
            float x = box.X + box.Width / 2, y = box.Y + box.Height / 2;
            float direction = previous ? -1.f : 1.f;
            Pen pen(enabled ? secondary : (dark ? Color(255, 83, 83, 91) : Color(255, 196, 196, 203)), 1.6f * s);
            pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);
            PointF points[] = { { x - direction * 2 * s, y - 4 * s }, { x + direction * 2 * s, y }, { x - direction * 2 * s, y + 4 * s } };
            graphics.DrawLines(&pen, points, 3);
        };
        chevron(layout.previous, true, composition.page > 0, -2);
        chevron(layout.next, false, !composition.lastPage, -3);
    }
}

Layout measure(const ChineseComposition& composition, unsigned dpi, int maxWidth) {
    initialize();
    Layout layout;
    float s = layout.scale = dpi / 96.f;
    int shadow = (int)std::ceil(12 * s), padding = (int)std::ceil(8 * s);
    int header = (int)std::ceil(18 * s), height = (int)std::ceil(28 * s), gap = (int)std::ceil(2 * s);
    int available = (std::max)((int)(120 * s), maxWidth - shadow * 2 - padding * 2);
    Gdiplus::Bitmap canvas(1, 1, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&canvas);
    Gdiplus::Font number(L"Segoe UI", 11 * s, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font candidate(L"Microsoft YaHei UI", 17 * s, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font comment(L"Segoe UI", 11 * s, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font preedit(L"Segoe UI", 13 * s, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    int left = shadow + padding, x = left, y = shadow + padding + header + (int)(4 * s), right = left;
    for (size_t i = 0; i < composition.candidates.size(); ++i) {
        float width = 20 * s + textWidth(graphics, label(composition, i), number) + textWidth(graphics, composition.candidates[i], candidate);
        if (i < composition.comments.size() && !composition.comments[i].empty()) width += 4 * s + textWidth(graphics, composition.comments[i], comment);
        int cellWidth = (std::min)((int)std::ceil(width), available);
        if (x > left && x + cellWidth > left + available) { x = left; y += height + gap; }
        layout.cells.push_back({ x, y, x + cellWidth, y + height });
        x += cellWidth + gap; right = (std::max)(right, x - gap);
    }
    if (!composition.candidates.empty() && (composition.page > 0 || !composition.lastPage)) {
        int pager = (int)std::ceil(36 * s);
        if (x + pager > left + available) { x = left; y += height + gap; }
        layout.previous = { x, y, x + pager / 2, y + height };
        layout.next = { x + pager / 2, y, x + pager, y + height };
        right = (std::max)(right, x + pager);
    }
    right = (std::max)(right, left + (std::min)(available, (int)textWidth(graphics, composition.preedit, preedit)));
    right = (std::max)(right, left + (int)(104 * s));
    int bottom = composition.candidates.empty() ? shadow + padding + header : y + height;
    layout.surface = { shadow, shadow, right + padding, bottom + padding };
    layout.preedit = { left + (int)(8 * s), shadow + padding, right, shadow + padding + header };
    layout.width = layout.surface.right + shadow;
    layout.height = layout.surface.bottom + shadow;
    return layout;
}

HBITMAP render(const ChineseComposition& composition, const Layout& layout, bool dark, int hover) {
    initialize();
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = layout.width;
    info.bmiHeader.biHeight = -layout.height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) return nullptr;
    Gdiplus::Bitmap canvas(layout.width, layout.height, layout.width * 4, PixelFormat32bppPARGB, (BYTE*)pixels);
    Gdiplus::Graphics graphics(&canvas);
    draw(graphics, composition, layout, dark, hover);
    graphics.Flush(Gdiplus::FlushIntentionSync);
    return bitmap;
}

int hitTest(const Layout& layout, POINT point) {
    for (size_t i = 0; i < layout.cells.size(); ++i) if (PtInRect(&layout.cells[i], point)) return (int)i;
    if (PtInRect(&layout.previous, point)) return -2;
    if (PtInRect(&layout.next, point)) return -3;
    return -1;
}

bool darkAppearance() {
    DWORD light = 1, size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return light == 0;
}

bool savePreview(const ChineseComposition& composition, const Layout& layout, bool dark, const wchar_t* path) {
    initialize();
    Gdiplus::Bitmap bitmap(layout.width, layout.height, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&bitmap);
    draw(graphics, composition, layout, dark, -1);
    graphics.Flush(Gdiplus::FlushIntentionSync);
    const CLSID png = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
    return bitmap.Save(path, &png) == Gdiplus::Ok;
}
}
