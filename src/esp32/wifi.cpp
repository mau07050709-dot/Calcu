#include "wifi.hpp"
#include "config.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <ctime>

namespace calcu {
void Wifi::begin() {
    configured_ = config::network.ssid[0] != '\0';
    if (!configured_) return;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
}

void Wifi::tick(uint32_t now) {
    if (!configured_) return;
    const bool online = connected();
    const auto attempt = retry_.tick(now, online);
    if (online) {
        hadConnection_ = true;
        if (!clockStarted_) {
            configTime(0, 0, config::network.timeServer);
            clockStarted_ = true;
            clockStartedAt_ = now;
        }
        clockTimedOut_ = now - clockStartedAt_ >= 20000;
        return;
    }

    clockStarted_ = false;
    clockTimedOut_ = false;
    if (attempt == WifiAttempt::first || (attempt == WifiAttempt::reconnect && !WiFi.reconnect())) {
        WiFi.begin(config::network.ssid, config::network.password);
    }
}

bool Wifi::connected() const {
    return configured_ && WiFi.status() == WL_CONNECTED;
}

bool Wifi::ready() const {
    return connected() && std::time(nullptr) >= 1704067200;
}

const char* Wifi::label() const {
    if (!configured_) return "WiFi sin config.";
    if (!connected()) {
        switch (WiFi.status()) {
            case WL_NO_SSID_AVAIL: return "Red no hallada";
            case WL_CONNECT_FAILED: return "WiFi no conecta";
            default: return hadConnection_ ? "Reconectando" : "Conectando WiFi";
        }
    }
    return ready() ? "WiFi listo" : clockTimedOut_ ? "Reloj sin NTP" : "Ajustando reloj";
}
}
