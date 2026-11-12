#pragma once

#include <cstdint>

namespace calcu {
enum class WifiAttempt { none, first, reconnect };

class WifiRetry {
public:
    WifiAttempt tick(uint32_t now, bool connected) {
        if (connected) {
            wasConnected_ = true;
            return WifiAttempt::none;
        }
        if (attempted_ && !wasConnected_ && now - lastAttempt_ < 15000) return WifiAttempt::none;
        const auto action = attempted_ ? WifiAttempt::reconnect : WifiAttempt::first;
        wasConnected_ = false;
        attempted_ = true;
        lastAttempt_ = now;
        return action;
    }

private:
    bool attempted_ = false;
    bool wasConnected_ = false;
    uint32_t lastAttempt_ = 0;
};
}
