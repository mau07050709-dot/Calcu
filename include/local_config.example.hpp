#pragma once

namespace calcu::config {
inline constexpr Hardware hardware{
    false,                     // Matriz 4x3 confirmada y habilitada.
    {{-1, -1, -1, -1}},          // GPIO filas.
    {{-1, -1, -1}},              // GPIO columnas con pull-up.
    {{'1','2','3','4','5','6','7','8','9','*','0','#'}},
    false,                     // LCD compatible HD44780 paralelo confirmado.
    -1, -1,                    // RS y E.
    {{-1, -1, -1, -1}}           // D4 a D7.
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
