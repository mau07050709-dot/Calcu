#pragma once

#include "calculator.hpp"
#include "desktop_chat.hpp"
#include "keypad.hpp"

namespace calcu::desktop {

struct Settings {
    Shortcut shortcut{'*', '#'};
    Angle angle = Angle::degrees;
    ResultFormat format = ResultFormat::decimal;
    Connection connection;

    static Settings load(const std::wstring& file);
    bool save(const std::wstring& file) const;
};

std::wstring settingsPath();

}
