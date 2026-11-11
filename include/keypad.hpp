#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace calcu {
enum class Action { none, digit, enter, mode, paletteOrNumbers, left, right,
                    erase, clear, angleOrCommit, symbol, cursorLeft, cursorRight, resultFormat, historyOlder, historyNewer };

struct KeyEvent {
    Action action = Action::none;
    char digit = '\0';
    std::string_view symbol{};
};

struct Shortcut {
    char prefix;
    char mode;

    constexpr bool valid() const {
        return (prefix == '*' || prefix == '#') &&
               (mode == '*' || mode == '#' || mode == '0' || (mode >= '7' && mode <= '9'));
    }
    constexpr char enter() const { return prefix == '*' ? '#' : '*'; }
};

class Keypad {
public:
    Keypad();
    bool configure(Shortcut shortcut);
    Shortcut shortcut() const { return shortcut_; }
    void cancelShortcut() { prefixPending_ = false; }
    KeyEvent press(char key, uint32_t now);
    void tick(uint32_t now);
    bool shortcutPending() const { return prefixPending_; }

private:
    Shortcut shortcut_;
    bool prefixPending_ = false;
    uint32_t prefixAt_ = 0;
};

class MultiTap {
public:
    void press(char key, uint32_t now);
    void tick(uint32_t now);
    void commit();
    void erase();
    void clear();
    void toggleNumbers();
    void move(int direction);
    std::size_t cursor() const { return cursor_; }

    const std::string& text() const { return text_; }
    bool numbers() const { return numbers_; }

private:
    std::string text_;
    std::size_t cursor_ = 0;
    bool numbers_ = false;
    char pendingKey_ = '\0';
    std::size_t choice_ = 0;
    uint32_t pressedAt_ = 0;
};
}
