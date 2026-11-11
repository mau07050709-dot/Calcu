#include "display.hpp"

#include <utility>

namespace calcu {
void Display::show(Frame frame) {
    for (auto& row : frame) row.resize(lcdWidth, ' ');
    if (frame == previous_) return;
    write(frame, previous_);
    previous_ = std::move(frame);
}

void Pages::set(std::string_view text) {
    text_.assign(text);
    offset_ = 0;
}

void Pages::move(int direction) {
    constexpr auto pageSize = lcdWidth * lcdRows;
    if (direction > 0 && offset_ + pageSize < text_.size()) offset_ += pageSize;
    if (direction < 0 && offset_ >= pageSize) offset_ -= pageSize;
}

Frame Pages::frame() const {
    Frame rows;
    for (std::size_t row = 0; row < lcdRows; ++row) {
        const auto start = offset_ + row * lcdWidth;
        if (start < text_.size()) rows[row] = text_.substr(start, lcdWidth);
    }
    return rows;
}
}
