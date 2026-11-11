#include "hardware.hpp"

#include <driver/gpio.h>

namespace calcu {
namespace {
constexpr auto& hw = config::hardware;

bool outputPin(int pin) {
    return pin >= 0 && pin < GPIO_NUM_MAX && GPIO_IS_VALID_OUTPUT_GPIO(pin);
}

bool inputPin(int pin) {
    return pin >= 0 && pin < GPIO_NUM_MAX && GPIO_IS_VALID_GPIO(pin);
}
}

bool BoardKeypad::begin() {
    if (!hw.matrixEnabled) return true;
    for (int row : hw.rows) if (!outputPin(row)) return false;
    for (int column : hw.columns) if (!inputPin(column)) return false;
    for (int row : hw.rows) pinMode(row, INPUT);
    for (int column : hw.columns) pinMode(column, INPUT_PULLUP);
    matrix_ = true;
    return true;
}

char BoardKeypad::scan() const {
    char pressed = '\0';
    for (std::size_t row = 0; row < hw.rows.size(); ++row) {
        const int pin = hw.rows[row];
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
        delayMicroseconds(4);
        for (std::size_t column = 0; column < hw.columns.size(); ++column) {
            if (digitalRead(hw.columns[column]) != LOW) continue;
            pressed = pressed == '\0' ? hw.keys[row * hw.columns.size() + column] : '\x7f';
        }
        pinMode(pin, INPUT);
    }
    return pressed;
}

char BoardKeypad::poll(uint32_t now) {
    if (Serial.available() > 0) return static_cast<char>(Serial.read());
    if (!matrix_ || now - scannedAt_ < 5) return '\0';

    scannedAt_ = now;
    const char current = scan();
    if (current != candidate_) {
        candidate_ = current;
        changedAt_ = now;
    } else if (current != stable_ && now - changedAt_ >= 25) {
        const bool released = stable_ == '\0';
        stable_ = current;
        if (released && current != '\x7f') return current;
    }
    return '\0';
}

bool BoardDisplay::begin() {
    if (!hw.parallelLcdEnabled) return true;
    if (!outputPin(hw.lcdRs) || !outputPin(hw.lcdEnable)) return false;
    for (int pin : hw.lcdData) if (!outputPin(pin)) return false;
    pinMode(hw.lcdRs, OUTPUT);
    pinMode(hw.lcdEnable, OUTPUT);
    digitalWrite(hw.lcdRs, LOW);
    digitalWrite(hw.lcdEnable, LOW);
    for (int pin : hw.lcdData) pinMode(pin, OUTPUT);

    delay(50);
    nibble(3);
    delay(5);
    nibble(3);
    delayMicroseconds(150);
    nibble(3);
    delayMicroseconds(150);
    nibble(2);
    delayMicroseconds(150);
    send(0x28);
    send(0x08);
    send(0x01);
    delay(2);
    send(0x06);
    send(0x0c);
    lcd_ = true;
    return true;
}

void BoardDisplay::nibble(uint8_t value) {
    for (unsigned bit = 0; bit < hw.lcdData.size(); ++bit) {
        digitalWrite(hw.lcdData[bit], (value >> bit) & 1);
    }
    delayMicroseconds(1);
    digitalWrite(hw.lcdEnable, HIGH);
    delayMicroseconds(1);
    digitalWrite(hw.lcdEnable, LOW);
}

void BoardDisplay::send(uint8_t value, bool data) {
    digitalWrite(hw.lcdRs, data ? HIGH : LOW);
    nibble(value >> 4);
    nibble(value & 0x0f);
    delayMicroseconds(45);
}

void BoardDisplay::write(const Frame& frame, const Frame& previous) {
    if (!lcd_) {
        Serial.printf("+----------------+\n|%s|\n|%s|\n+----------------+\n",
                      frame[0].c_str(), frame[1].c_str());
        return;
    }
    for (std::size_t row = 0; row < lcdRows; ++row) {
        if (frame[row] == previous[row]) continue;
        send(row == 0 ? 0x80 : 0xc0);
        for (char cell : frame[row]) send(static_cast<uint8_t>(cell), true);
    }
}
}
