#include "hardware_profile.hpp"

#include <algorithm>
#include <string_view>

namespace calcu {
namespace {
template<std::size_t Size>
bool unique(const std::array<int, Size>& pins) {
    for (auto pin = pins.begin(); pin != pins.end(); ++pin) {
        if (std::find(pin + 1, pins.end(), *pin) != pins.end()) return false;
    }
    return true;
}

bool logicalKeymap(const std::array<char, 12>& keys) {
    for (const auto key : std::string_view("0123456789*#")) {
        if (std::count(keys.begin(), keys.end(), key) != 1) return false;
    }
    return true;
}
}

HardwareValidation validateHardware(const config::Hardware& profile, PinCheck output, PinCheck input) {
    HardwareValidation valid;
    const std::array<int, 7> keypad{
        profile.rows[0], profile.rows[1], profile.rows[2], profile.rows[3],
        profile.columns[0], profile.columns[1], profile.columns[2]
    };
    const std::array<int, 6> lcd{
        profile.lcdRs, profile.lcdEnable, profile.lcdData[0], profile.lcdData[1],
        profile.lcdData[2], profile.lcdData[3]
    };
    if (profile.matrixEnabled) {
        valid.keypad = unique(keypad) && logicalKeymap(profile.keys) &&
            std::all_of(profile.rows.begin(), profile.rows.end(), output) &&
            std::all_of(profile.columns.begin(), profile.columns.end(),
                        profile.matrixInternalPullups ? output : input);
    }
    if (profile.usesParallelLcd()) {
        const auto first = profile.lcdRowAddresses[0], second = profile.lcdRowAddresses[1];
        const auto validRow = [](uint8_t address) {
            return address <= 0x18 || (address >= 0x40 && address <= 0x58);
        };
        valid.lcd = unique(lcd) && std::all_of(lcd.begin(), lcd.end(), output) &&
            validRow(first) && validRow(second) && (first + 16 <= second || second + 16 <= first);
    }
    if (profile.matrixEnabled && profile.usesParallelLcd() && valid.keypad && valid.lcd) {
        for (const auto pin : keypad) {
            if (std::find(lcd.begin(), lcd.end(), pin) != lcd.end()) return {false, false};
        }
    }
    return valid;
}
}
