#include "scientific_display.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

namespace calcu::desktop {
namespace {
struct Glyph {
    int x, y;
    HFONT font;
    std::wstring text;
};

struct Stroke { int x, y, x2, y2; };
struct Anchor { std::size_t offset; int x, y, height; };

struct Box {
    int width = 0, above = 0, below = 0;
    std::vector<Glyph> glyphs;
    std::vector<Stroke> strokes;
    std::vector<Anchor> anchors;

    void add(Box child, int x, int y) {
        width = std::max(width, x + child.width);
        above = std::max(above, child.above - y);
        below = std::max(below, child.below + y);
        for (auto& glyph : child.glyphs) {
            glyph.x += x;
            glyph.y += y;
            glyphs.push_back(std::move(glyph));
        }
        for (auto line : child.strokes) {
            line.x += x; line.x2 += x;
            line.y += y; line.y2 += y;
            strokes.push_back(line);
        }
        for (auto anchor : child.anchors) {
            anchor.x += x;
            anchor.y += y;
            anchors.push_back(anchor);
        }
    }
};

template<class Fonts>
class Layout {
public:
    Layout(HDC dc, std::string_view source, Fonts fonts)
        : dc_(dc), source_(source), fonts_(fonts), tree_(parseExpression(source)) {}

    Box make(int size) {
        if (source_.empty()) return plain({}, size, 0);
        if (!tree_.consumed || tree_.error == MathError::complexity) return linear(size);
        return node(tree_.root, size);
    }

    Box linear(int size) { return plain(source_, size, 0); }
    Box caption(std::wstring text, int size) { return glyph(std::move(text), size); }

private:
    Box glyph(std::wstring text, int size) {
        Box box;
        const auto font = fonts_(size);
        auto old = SelectObject(dc_, font);
        SIZE width{};
        GetTextExtentPoint32W(dc_, text.c_str(), static_cast<int>(text.size()), &width);
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc_, &metrics);
        SelectObject(dc_, old);
        box.width = width.cx;
        box.above = metrics.tmAscent;
        box.below = metrics.tmDescent;
        box.glyphs.push_back({0, 0, font, std::move(text)});
        return box;
    }

    static wchar_t asMath(char ch) {
        switch (ch) {
            case '*': return L'×';
            case '/': return L'÷';
            case '-': return L'−';
            default: return static_cast<wchar_t>(ch);
        }
    }

    void endPoints(Box& box, std::size_t begin, std::size_t end) {
        box.anchors.push_back({begin, 0, 0, box.above});
        box.anchors.push_back({end, box.width, 0, box.above});
    }

    Box plain(std::string_view text, int size, std::size_t begin) {
        std::wstring wide;
        for (char ch : text) wide += asMath(ch);
        auto box = glyph(wide, size);
        auto old = SelectObject(dc_, box.glyphs.front().font);
        for (std::size_t i = 0; i <= wide.size(); ++i) {
            SIZE prefix{};
            GetTextExtentPoint32W(dc_, wide.data(), static_cast<int>(i), &prefix);
            box.anchors.push_back({begin + i, prefix.cx, 0, box.above});
        }
        SelectObject(dc_, old);
        return box;
    }

    Box symbol(std::wstring text, int size, std::size_t begin, std::size_t end) {
        auto box = glyph(std::move(text), size);
        endPoints(box, begin, end);
        return box;
    }

    Box script(Box base, Box value, int size, bool lower = false) {
        Box out;
        const int x = base.width + std::max(1, size / 15);
        const int y = lower ? size / 4 : -std::max(size / 2, base.above / 2);
        out.add(std::move(base), 0, 0);
        out.add(std::move(value), x, y);
        return out;
    }

    Box fraction(Box numerator, Box denominator, int size) {
        Box out;
        const int padding = std::max(3, size / 5);
        const int gap = std::max(2, size / 8);
        const int width = std::max(numerator.width, denominator.width) + padding * 2;
        const int bar = -size / 5;
        const int nx = (width - numerator.width) / 2, ny = bar - gap - numerator.below;
        const int dx = (width - denominator.width) / 2, dy = bar + gap + denominator.above;
        out.add(std::move(numerator), nx, ny);
        out.add(std::move(denominator), dx, dy);
        out.width = width;
        out.strokes.push_back({1, bar, width - 1, bar});
        return out;
    }

    Box radical(Box radicand, Box index, int size) {
        Box out;
        const int gap = std::max(2, size / 9);
        const int start = std::max(size / 4, index.width);
        const int bar = -radicand.above - gap;
        const int left = start + size / 2 + gap;
        out.add(std::move(radicand), left, 0);
        out.add(std::move(index), 0, bar + size / 3);
        out.above = std::max(out.above, -bar);
        out.width += gap;
        out.strokes.push_back({start - 4, -size / 5, start, -size / 5});
        out.strokes.push_back({start, -size / 5, start + size / 6, gap});
        out.strokes.push_back({start + size / 6, gap, left - gap, bar});
        out.strokes.push_back({left - gap, bar, out.width - 1, bar});
        return out;
    }

    Box group(Box content, int size, std::size_t open, std::size_t close, bool closed) {
        const int parenSize = std::max(size, content.above + content.below - size / 3);
        const int middle = (content.below - content.above) / 2;
        Box out;
        const int center = middle + parenSize / 3;
        out.add(plain("(", parenSize, open), 0, center);
        out.add(std::move(content), out.width, 0);
        if (closed) out.add(plain(")", parenSize, close), out.width, center);
        return out;
    }

    Box node(int id, int size) {
        const auto& n = tree_.nodes[id];
        const auto text = source_.substr(n.begin, n.end - n.begin);
        const int small = std::max(8, size * 7 / 10);
        switch (n.kind) {
            case NodeKind::literal: {
                const auto exponent = text.find_first_of("eE");
                if (exponent == std::string_view::npos || exponent + 1 == text.size() ||
                    text.back() == '+' || text.back() == '-') return plain(text, size, n.begin);
                auto base = plain(text.substr(0, exponent), size, n.begin);
                base.add(symbol(L" × 10", size, n.begin + exponent, n.begin + exponent + 1), base.width, 0);
                return script(std::move(base), plain(text.substr(exponent + 1), small, n.begin + exponent + 1), size);
            }
            case NodeKind::name:
                if (text == "pi") return symbol(L"π", size, n.begin, n.end);
                return plain(text, size, n.begin);
            case NodeKind::placeholder: {
                auto box = glyph(L"□", size);
                box.anchors.push_back({n.begin, box.width / 2, 0, box.above});
                return box;
            }
            case NodeKind::group: return group(node(n.left, size), size, n.begin, n.end - 1, n.closed);
            case NodeKind::prefix: {
                auto sign = plain(source_.substr(n.marker, 1), size, n.marker);
                sign.add(node(n.left, size), sign.width, 0);
                return sign;
            }
            case NodeKind::postfix: {
                auto value = node(n.left, size);
                value.add(plain(source_.substr(n.marker, 1), size, n.marker), value.width, 0);
                return value;
            }
            case NodeKind::binary: {
                const char op = source_[n.marker];
                if (op == '/') return fraction(node(n.left, small), node(n.right, small), size);
                if (op == '^') return script(node(n.left, size), node(n.right, small), size);
                auto left = node(n.left, size);
                const int gap = std::max(2, size / 9);
                left.add(plain(source_.substr(n.marker, 1), size, n.marker), left.width + gap, 0);
                left.add(node(n.right, size), left.width + gap, 0);
                return left;
            }
            case NodeKind::call: return call(n, size, small);
        }
        return {};
    }

    Box call(const ExpressionNode& n, int size, int small) {
        const auto name = source_.substr(n.begin, n.marker - n.begin);
        const auto open = source_.find('(', n.marker);
        if ((name == "sqrt" || name == "cbrt") && n.right < 0) {
            auto index = name == "sqrt" ? Box{} : glyph(L"3", small);
            auto box = radical(node(n.left, size), std::move(index), size);
            box.anchors.push_back({n.begin, 0, 0, box.above});
            if (n.closed) box.anchors.push_back({n.end, box.width, 0, box.above});
            return box;
        }
        if (name == "root") {
            auto index = n.right < 0 ? glyph(L"□", small) : node(n.right, small);
            auto box = radical(node(n.left, size), std::move(index), size);
            box.anchors.push_back({n.begin, 0, 0, box.above});
            if (n.closed) box.anchors.push_back({n.end, box.width, 0, box.above});
            return box;
        }
        auto value = node(n.left, size);
        if ((name == "log" || name == "logb") && n.right >= 0) {
            auto title = symbol(L"log", size, n.begin, n.marker);
            title = script(std::move(title), node(n.right, small), size, true);
            title.add(group(std::move(value), size, open, n.end - 1, n.closed), title.width, 0);
            return title;
        }
        auto title = plain(name, size, n.begin);
        if (n.right >= 0) {
            const auto comma = source_.find(',', tree_.nodes[n.left].end);
            value.add(plain(",", size, comma), value.width, 0);
            value.add(node(n.right, size), value.width + size / 7, 0);
        }
        title.add(group(std::move(value), size, open, n.end - 1, n.closed), title.width, 0);
        return title;
    }

    HDC dc_;
    std::string_view source_;
    Fonts fonts_;
    ExpressionTree tree_;
};

void draw(HDC dc, const Box& box, RECT bounds, int left, int baseline, COLORREF color) {
    const int state = SaveDC(dc);
    IntersectClipRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    SetTextAlign(dc, TA_LEFT | TA_BASELINE);
    const auto pen = CreatePen(PS_SOLID, 1, color);
    SelectObject(dc, pen);
    for (const auto& glyph : box.glyphs) {
        SelectObject(dc, glyph.font);
        TextOutW(dc, left + glyph.x, baseline + glyph.y, glyph.text.c_str(), static_cast<int>(glyph.text.size()));
    }
    for (const auto& line : box.strokes) {
        MoveToEx(dc, left + line.x, baseline + line.y, nullptr);
        LineTo(dc, left + line.x2, baseline + line.y2);
    }
    RestoreDC(dc, state);
    DeleteObject(pen);
}

int centered(RECT rect, const Box& box) {
    return (rect.top + rect.bottom + box.above - box.below) / 2;
}

template<class View>
Box fit(View& layout, RECT viewport, int size, int minimum, int maxWidth = 0) {
    for (int current = size; current >= minimum; current -= 2) {
        auto box = layout.make(current);
        if (box.above + box.below <= viewport.bottom - viewport.top && (!maxWidth || box.width <= maxWidth)) {
            return box;
        }
    }
    return layout.linear(minimum);
}
}

ScientificDisplay::~ScientificDisplay() {
    for (const auto& entry : fonts_) DeleteObject(entry.second);
}

HFONT ScientificDisplay::font(int height) {
    auto& result = fonts_[height];
    if (!result) {
        result = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Cambria");
    }
    return result;
}

void ScientificDisplay::paint(HDC dc, RECT input, RECT output, const App& app, int dpi) {
    constexpr COLORREF text = RGB(32, 57, 33);
    const auto scale = [&](int px) { return MulDiv(px, dpi, 96); };
    const auto fontFor = [this](int size) { return font(size); };
    const auto* entry = app.historyEntry();
    const auto source = entry ? std::string_view(entry->expression) : app.expression();
    Layout expression(dc, source, fontFor);
    auto box = fit(expression, input, scale(33), scale(15));
    const int baseline = centered(input, box);
    const int viewport = input.right - input.left;
    Anchor caret{0, 0, 0, box.above};
    auto distance = std::numeric_limits<std::size_t>::max();
    for (const auto& anchor : box.anchors) {
        const auto current = app.cursor() > anchor.offset ? app.cursor() - anchor.offset : anchor.offset - app.cursor();
        if (current <= distance) {
            distance = current;
            caret = anchor;
        }
    }
    if (entry) inputScroll_ = 0;
    else {
        inputScroll_ = std::clamp(inputScroll_, 0, std::max(0, box.width - viewport + scale(12)));
        if (caret.x - inputScroll_ < 0) inputScroll_ = caret.x;
        if (caret.x - inputScroll_ >= viewport - scale(8)) inputScroll_ = caret.x - viewport + scale(8);
    }
    const int origin = input.left - inputScroll_;
    draw(dc, box, input, origin, baseline, text);
    anchors_.clear();
    for (const auto& anchor : box.anchors) {
        if (origin + anchor.x >= input.left && origin + anchor.x < input.right) {
            anchors_.push_back({anchor.offset, origin + anchor.x, baseline + anchor.y, anchor.height});
        }
    }
    if (!app.visibleResult() && !app.showingPalette()) {
        const int saved = SaveDC(dc);
        IntersectClipRect(dc, input.left, input.top, input.right, input.bottom);
        const auto pen = CreatePen(PS_SOLID, scale(2), RGB(40, 92, 61));
        SelectObject(dc, pen);
        MoveToEx(dc, origin + caret.x, baseline + caret.y - caret.height + 2, nullptr);
        LineTo(dc, origin + caret.x, baseline + caret.y + 4);
        RestoreDC(dc, saved);
        DeleteObject(pen);
    }

    if (const auto* result = app.visibleResult()) {
        const auto formatted = formatResult(*result, app.resultFormat());
        Layout display(dc, formatted, fontFor);
        const int available = output.right - output.left - scale(30);
        const bool error = result->error != MathError::none;
        auto rendered = error ? display.linear(scale(23)) :
            fit(display, output, scale(31), scale(14), available);
        const auto displayed = error ? Evaluation{} : evaluate(formatted, Angle::degrees);
        const bool approximate = !error && (!result->exact || !displayed.exact ||
            result->exact->numerator != displayed.exact->numerator ||
            result->exact->denominator != displayed.exact->denominator);
        auto combined = display.caption(error ? L"" : approximate ? L"≈" : L"=", scale(24));
        combined.add(std::move(rendered), combined.width + (error ? 0 : scale(10)), 0);
        draw(dc, combined, output, std::max(static_cast<int>(output.left), static_cast<int>(output.right) - combined.width),
            centered(output, combined), error ? RGB(119, 35, 32) : text);
    }
}

std::size_t ScientificDisplay::nearest(POINT position) const {
    std::size_t offset = 0;
    long best = std::numeric_limits<long>::max();
    for (const auto& anchor : anchors_) {
        const auto y = std::clamp(position.y, static_cast<LONG>(anchor.y - anchor.height),
                                  static_cast<LONG>(anchor.y + 4));
        const auto distance = std::abs(position.x - anchor.x) + 3 * std::abs(position.y - y);
        if (distance < best) {
            best = distance;
            offset = anchor.offset;
        }
    }
    return offset;
}
}
