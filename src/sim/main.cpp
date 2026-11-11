#ifndef PIO_UNIT_TESTING
#include "app.hpp"
#include "config.hpp"

#include <charconv>
#include <iostream>
#include <utility>

namespace {
class ConsoleDisplay : public calcu::Display {
    void write(const calcu::Frame& frame, const calcu::Frame&) override {
        std::cout << "+----------------+\n|" << frame[0] << "|\n|"
                  << frame[1] << "|\n+----------------+\n";
    }
};

class SimulatedChat : public calcu::Chat {
public:
    calcu::ChatReply start(std::string_view question, uint32_t now) override {
        answer_ = "Demo local. Recibi: " + std::string(question) +
                  ". Usa *3 para avanzar y *2 para retroceder en esta respuesta.";
        startedAt_ = now;
        return {calcu::ChatStatus::pending, {}};
    }

    calcu::ChatReply poll(uint32_t now) override {
        if (now - startedAt_ < 400) return {calcu::ChatStatus::pending, {}};
        return {calcu::ChatStatus::answer, std::move(answer_)};
    }

    const char* connectionLabel() const override { return "Simulacion local"; }

private:
    uint32_t startedAt_ = 0;
    std::string answer_;
};
}

int main() {
    ConsoleDisplay display;
    SimulatedChat chat;
    calcu::App app(display, chat);
    uint32_t now = 0;

    std::cout << "Calcu | Teclas 0-9, * y # | :wait MS | :quit\n"
                 "*# modo, *1 simbolos/123, *2/*3 mover, *4 borrar, *5 limpiar,\n"
                 "*6 DEG/RAD o confirmar letra, # evaluar/enviar.\n";
    app.tick(now);
    for (std::string line; std::getline(std::cin, line);) {
        if (line == ":quit") break;
        if (line.compare(0, 6, ":wait ") == 0) {
            uint32_t duration = 0;
            const auto end = line.data() + line.size();
            const auto parsed = std::from_chars(line.data() + 6, end, duration);
            if (parsed.ec != std::errc{} || parsed.ptr != end || duration > 60000) {
                std::cout << "Usa :wait con 0 a 60000 milisegundos.\n";
                continue;
            }
            now += duration;
            app.tick(now);
            continue;
        }
        if (line.find_first_not_of("0123456789*# \t") != std::string::npos) {
            std::cout << "Usa teclas 0-9, *, # o un comando.\n";
            continue;
        }
        for (char key : line) {
            if (key == ' ' || key == '\t') continue;
            now += 40;
            app.press(key, now);
        }
        now += calcu::config::multiTapMs;
        app.tick(now);
    }
}
#endif
