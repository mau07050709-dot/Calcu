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
    if (connected()) {
        if (!clockStarted_) {
            configTime(0, 0, config::network.timeServer);
            clockStarted_ = true;
        }
        return;
    }

    clockStarted_ = false;
    if (!attempted_ || now - lastAttempt_ >= 15000) {
        attempted_ = true;
        lastAttempt_ = now;
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
    if (!connected()) return "Conectando WiFi";
    return ready() ? "WiFi listo" : "Ajustando reloj";
}
}
