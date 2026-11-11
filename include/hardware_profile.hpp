#pragma once

#include "config.hpp"

namespace calcu {
using PinCheck = bool (*)(int);

struct HardwareValidation {
    bool keypad = true;
    bool lcd = true;
};

HardwareValidation validateHardware(const config::Hardware& profile, PinCheck output, PinCheck input);
}
