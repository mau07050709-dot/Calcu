#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace calcu {
inline constexpr std::size_t lcdWidth = 16;
inline constexpr std::size_t lcdRows = 2;
using Frame = std::array<std::string, lcdRows>;

class Display {
public:
    virtual ~Display() = default;
    void show(Frame frame);

protected:
    virtual void write(const Frame& frame, const Frame& previous) = 0;

private:
    Frame previous_;
};

class Pages {
public:
    void set(std::string_view text);
    void move(int direction);
    Frame frame() const;

private:
    std::string text_;
    std::size_t offset_ = 0;
};
}
