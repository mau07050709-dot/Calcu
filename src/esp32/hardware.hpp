#pragma once

#include "config.hpp"
#include "display.hpp"

#include <Arduino.h>

namespace calcu {
class BoardKeypad {
public:
    bool begin();
    char poll(uint32_t now);

private:
    char scan() const;
    bool matrix_ = false;
    char candidate_ = '\0';
    char stable_ = '\0';
    uint32_t changedAt_ = 0;
    uint32_t scannedAt_ = 0;
};

class BoardDisplay : public Display {
public:
    bool begin();

private:
    void write(const Frame& frame, const Frame& previous) override;
    void nibble(uint8_t value);
    void send(uint8_t value, bool data = false);
    bool lcd_ = false;
};
}
