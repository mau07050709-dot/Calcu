#include "desktop_chat.hpp"
#include "settings.hpp"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace calcu;
using namespace calcu::desktop;

void check(bool ok, const char* label) {
    if (!ok) throw std::runtime_error(label);
}

struct TemporarySettings {
    explicit TemporarySettings(std::filesystem::path path) : file(std::move(path)) {}
    ~TemporarySettings() { DeleteFileW(file.c_str()); }
    std::filesystem::path file;
};
}

int main(int argc, char** argv) {
    try {
        if (argc == 3) {
            const std::string_view url(argv[1]), token(argv[2]);
            const Connection local{false, {url.begin(), url.end()}, {token.begin(), token.end()}};
            DesktopChat chat;
            check(chat.check(local, GetTickCount()).status == ChatStatus::pending, "Sonda TLS asincrona");
            ConnectionCheck probe{ChatStatus::pending, 0, {}};
            while (probe.status == ChatStatus::pending) {
                Sleep(20);
                probe = chat.pollCheck(GetTickCount());
            }
            check(probe.status == ChatStatus::error && probe.completed == 0 &&
                  probe.detail.find("certificado TLS") != std::string::npos,
                  "WinHTTP rechaza un certificado sin confianza en el primer paso");
            chat.configure(local);
            check(chat.startWithHistory("PRUEBA", {{"ANTERIOR", "RESPUESTA"}}, GetTickCount()).status ==
                  ChatStatus::pending, "POST TLS asincrono");
            ChatReply reply{ChatStatus::pending, {}};
            do {
                Sleep(20);
                reply = chat.poll(GetTickCount());
            } while (reply.status == ChatStatus::pending);
            check(reply.status == ChatStatus::error && reply.text == "Certificado TLS",
                  "El POST tampoco omite la validacion");
            chat.configure({true, {}, {}});
            check(chat.start("PRUEBA", 1).status == ChatStatus::pending &&
                  chat.poll(301).status == ChatStatus::answer, "Recupera la interfaz tras fallo TLS");
            std::cout << "OK: WinHTTP rechaza TLS sin confianza en GET y POST\n";
            return 0;
        }
        const TemporarySettings test(std::filesystem::temp_directory_path() /
            (L"calcu-settings-" + std::to_wstring(GetCurrentProcessId()) + L".ini"));
        const Settings initial = Settings::load(test.file.wstring());
        check(initial.angle == Angle::degrees && !initial.connection.configured(), "Valores iniciales");

        Settings candidate;
        candidate.angle = Angle::radians;
        candidate.format = ResultFormat::fraction;
        candidate.shortcut = {'#', '8'};
        candidate.connection = {true, L"https://calcu.example.test:9443/ask",
                                L"token-ficticio-para-la-prueba-local-8328"};
        check(validUrl(candidate.connection.url), "URL HTTPS con puerto");
        check(!validUrl(L"http://calcu.example.test/ask"), "Rechaza HTTP");
        check(!validUrl(L"https://user:pass@calcu.example.test/ask"), "Rechaza credenciales en URL");
        check(!validUrl(L"https://calcu.example.test/ask#parte"), "Rechaza fragmentos");
        check(!validToken(L"token corto"), "Rechaza token corto");
        check(candidate.save(test.file.wstring()), "Escritura y proteccion DPAPI");
        std::ifstream source(test.file, std::ios::binary);
        const std::string stored((std::istreambuf_iterator<char>(source)), {});
        const auto* secret = reinterpret_cast<const char*>(candidate.connection.token.data());
        check(stored.find(std::string(secret, candidate.connection.token.size() * sizeof(wchar_t))) ==
            std::string::npos, "Token no se guarda en texto plano");
        source.close();
        const Settings restored = Settings::load(test.file.wstring());
        check(restored.angle == Angle::radians && restored.format == ResultFormat::fraction && restored.shortcut.prefix == '#' &&
            restored.shortcut.mode == '8' && restored.connection.demo &&
            restored.connection.url == candidate.connection.url &&
            restored.connection.token == candidate.connection.token, "Persistencia de configuracion");

        DesktopChat chat;
        check(chat.start("HOLA", 1).status == ChatStatus::error, "Aviso sin servidor");
        check(chat.check({}, 1).status == ChatStatus::error, "Sonda avisa si falta configuracion");
        chat.configure(restored.connection);
        check(chat.start("HOLA", 5).status == ChatStatus::pending, "Demostracion asincrona");
        check(chat.poll(304).status == ChatStatus::pending, "Espera de demostracion");
        const auto reply = chat.poll(305);
        check(reply.status == ChatStatus::answer && reply.text.size() > 64 &&
            reply.text.find("HOLA") != std::string::npos, "Respuesta local multipagina");
        std::cout << "OK: configuracion, DPAPI, validacion HTTPS y demostracion\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FALLO: " << error.what() << '\n';
        return 1;
    }
}
