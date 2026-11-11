#pragma once

#include "app.hpp"

#include <windows.h>
#include <map>
#include <vector>

namespace calcu::desktop {
class ScientificDisplay {
public:
    ~ScientificDisplay();
    void paint(HDC dc, RECT input, RECT output, const App& app, int dpi);
    std::size_t nearest(POINT position) const;

private:
    struct CursorPoint { std::size_t offset; int x, y, height; };
    HFONT font(int height);

    std::map<int, HFONT> fonts_;
    std::vector<CursorPoint> anchors_;
    int inputScroll_ = 0;
};
}
