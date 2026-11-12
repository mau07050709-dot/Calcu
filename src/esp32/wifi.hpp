#pragma once

#include "wifi_retry.hpp"

#include <cstdint>

namespace calcu {
class Wifi {
public:
    void begin();
    void tick(uint32_t now);
    bool connected() const;
    bool ready() const;
    const char* label() const;

private:
    bool configured_ = false;
    bool clockStarted_ = false;
    bool hadConnection_ = false;
    bool clockTimedOut_ = false;
    uint32_t clockStartedAt_ = 0;
    WifiRetry retry_;
};
}
