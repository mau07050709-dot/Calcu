#include "keypad.hpp"
#include "config.hpp"

#include <array>
#include <string_view>

namespace calcu {
namespace {
constexpr std::array<std::string_view, 10> characters{
    " ", ".,?!:;'-", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ"
};
}

Keypad::Keypad() : shortcut_{config::shortcutPrefix, config::modeKey} {
    static_assert(Shortcut{config::shortcutPrefix, config::modeKey}.valid());
}

bool Keypad::configure(Shortcut shortcut) {
    if (!shortcut.valid()) return false;
    shortcut_ = shortcut;
    cancelShortcut();
    return true;
}

void Keypad::tick(uint32_t now) {
    if (prefixPending_ && now - prefixAt_ >= config::shortcutMs) prefixPending_ = false;
}

KeyEvent Keypad::press(char key, uint32_t now) {
    tick(now);
    const bool digit = key >= '0' && key <= '9';
    if (!digit && key != '*' && key != '#') return {};

    if (prefixPending_) {
        prefixPending_ = false;
        if (key == shortcut_.mode) return {Action::mode};
        if (key == shortcut_.prefix) return {Action::erase};
        switch (key) {
            case '1': return {Action::paletteOrNumbers};
            case '2': return {Action::left};
            case '3': return {Action::right};
            case '4': return {Action::erase};
            case '5': return {Action::clear};
            case '6': return {Action::angleOrCommit};
            default: break;
        }
        constexpr std::array<char, 4> available{'7', '8', '9', '0'};
        constexpr std::array<Action, 3> actions{Action::cursorLeft, Action::cursorRight, Action::chatHistory};
        std::size_t index = 0;
        for (const char candidate : available) {
            if (candidate == shortcut_.mode) continue;
            if (index == actions.size()) break;
            if (key == candidate) return {actions[index]};
            ++index;
        }
        return {};
    }
    if (key == shortcut_.prefix) {
        prefixPending_ = true;
        prefixAt_ = now;
        return {};
    }
    if (digit) return {Action::digit, key};
    return {Action::enter};
}

void MultiTap::press(char key, uint32_t now) {
    if (key < '0' || key > '9') return;
    tick(now);
    const auto choices = characters[key - '0'];
    if (!numbers_ && key == pendingKey_) {
        choice_ = (choice_ + 1) % choices.size();
        text_[cursor_ - 1] = choices[choice_];
        pressedAt_ = now;
        return;
    }

    commit();
    if (text_.size() >= config::questionLimit) return;
    text_.insert(cursor_, 1, numbers_ ? key : choices.front());
    ++cursor_;
    if (!numbers_ && choices.size() > 1) {
        pendingKey_ = key;
        pressedAt_ = now;
    }
}

void MultiTap::tick(uint32_t now) {
    if (pendingKey_ != '\0' && now - pressedAt_ >= config::multiTapMs) commit();
}

void MultiTap::commit() {
    pendingKey_ = '\0';
    choice_ = 0;
}

void MultiTap::erase() {
    commit();
    if (cursor_ > 0) text_.erase(--cursor_, 1);
}

void MultiTap::clear() {
    commit();
    text_.clear();
    cursor_ = 0;
}

void MultiTap::move(int direction) {
    commit();
    if (direction < 0 && cursor_ > 0) --cursor_;
    if (direction > 0 && cursor_ < text_.size()) ++cursor_;
}

void MultiTap::toggleNumbers() {
    commit();
    numbers_ = !numbers_;
}
}
