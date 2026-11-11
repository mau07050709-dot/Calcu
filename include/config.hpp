#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace calcu::config {
inline constexpr std::size_t expressionLimit = 128;
inline constexpr std::size_t questionLimit = 240;
inline constexpr std::size_t answerLimit = 768;
inline constexpr std::size_t chatHistoryLimit = 4;
inline constexpr uint32_t multiTapMs = 750;
inline constexpr uint32_t shortcutMs = 1200;
inline constexpr char shortcutPrefix = '*';
inline constexpr char modeKey = '#';

enum class LcdController { legacyFlag, serial, hd44780Parallel };

struct Hardware {
    bool matrixEnabled = false;
    std::array<int, 4> rows{{-1, -1, -1, -1}};
    std::array<int, 3> columns{{-1, -1, -1}};
    // Orden fila/columna. Cambiar según las leyendas reales.
    std::array<char, 12> keys{{'1','2','3','4','5','6','7','8','9','*','0','#'}};

    bool parallelLcdEnabled = false;
    int lcdRs = -1;
    int lcdEnable = -1;
    std::array<int, 4> lcdData{{-1, -1, -1, -1}};
    LcdController lcdController = LcdController::legacyFlag;
    std::array<uint8_t, 2> lcdRowAddresses{{0x00, 0x40}};
    bool matrixInternalPullups = true;

    constexpr bool usesParallelLcd() const {
        return lcdController == LcdController::legacyFlag ? parallelLcdEnabled :
            lcdController == LcdController::hd44780Parallel;
    }
};

struct Network {
    const char* ssid = "";
    const char* password = "";
    const char* proxyUrl = "";
    const char* deviceToken = "";
    const char* rootCa = "";
    const char* timeServer = "pool.ntp.org";
};

static_assert(shortcutPrefix == '*' || shortcutPrefix == '#');

} // namespace calcu::config

#if __has_include("local_config.hpp")
#include "local_config.hpp"
#else
namespace calcu::config {
inline constexpr Hardware hardware{};
inline constexpr Network network{};
}
#endif
