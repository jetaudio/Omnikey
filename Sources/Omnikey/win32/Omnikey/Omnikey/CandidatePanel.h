#pragma once
#include "WindowsRime.h"

namespace CandidatePanel {
    struct Layout {
        int width = 0, height = 0;
        float scale = 1;
        RECT surface = {}, preedit = {}, previous = {}, next = {};
        std::vector<RECT> cells;
    };
    Layout measure(const ChineseComposition& composition, unsigned dpi, int maxWidth);
    // A premultiplied-alpha DIB for UpdateLayeredWindow; caller owns the bitmap.
    HBITMAP render(const ChineseComposition& composition, const Layout& layout, bool dark, int hover = -1);
    int hitTest(const Layout& layout, POINT point); // -2: previous, -3: next, -1: none
    bool darkAppearance();
    bool savePreview(const ChineseComposition& composition, const Layout& layout, bool dark, const wchar_t* path);
}
