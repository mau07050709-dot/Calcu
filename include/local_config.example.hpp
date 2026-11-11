#pragma once

namespace calcu::config {
inline constexpr Hardware hardware{
    false,                     // Matriz 4x3 confirmada y habilitada.
    {{-1, -1, -1, -1}},          // GPIO filas.
    {{-1, -1, -1}},              // GPIO columnas con pull-up.
    {{'1','2','3','4','5','6','7','8','9','*','0','#'}},
    false,                     // Indicador heredado. Se usa con LcdController::legacyFlag.
    -1, -1,                    // RS y E.
    {{-1, -1, -1, -1}},          // D4 a D7.
    LcdController::serial,      // Seleccionar hd44780Parallel solo si se confirma el controlador.
    {{0x00, 0x40}},              // Direcciones DDRAM de ambas filas para el controlador confirmado.
    true                       // Pull-up interno. false si la matriz usa pull-ups externos.
};

inline constexpr Network network{
    "", // SSID.
    "", // Contraseña de Wi-Fi.
    "", // URL HTTPS completa, incluida la ruta /ask.
    "", // Token del dispositivo para el intermediario.
    ""  // Certificado CA PEM para verificar el intermediario.
    // Sustituir por R"PEM(-----BEGIN CERTIFICATE-----
    // ...
    // -----END CERTIFICATE-----)PEM" si corresponde.
};
}
