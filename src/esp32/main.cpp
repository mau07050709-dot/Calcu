#include "app.hpp"
#include "hardware.hpp"
#include "proxy_chat.hpp"
#include "wifi.hpp"

namespace {
calcu::BoardKeypad keypad;
calcu::BoardDisplay display;
calcu::Wifi wifi;
calcu::ProxyChat chat(wifi);
calcu::App app(display, chat);
}

void setup() {
    Serial.begin(115200);
    if (!keypad.begin()) Serial.println("GPIO de teclado invalidos. Usando serie.");
    if (!display.begin()) Serial.println("GPIO de LCD invalidos. Usando serie.");
    wifi.begin();
    chat.begin();
}

void loop() {
    const uint32_t now = millis();
    wifi.tick(now);
    const char key = keypad.poll(now);
    if (key == '\0') app.tick(now);
    else app.press(key, now);
    delay(1);
}
