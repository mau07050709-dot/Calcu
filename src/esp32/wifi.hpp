#pragma once

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
    bool attempted_ = false;
    bool clockStarted_ = false;
    uint32_t lastAttempt_ = 0;
};
}
