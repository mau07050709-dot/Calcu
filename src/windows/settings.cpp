#include "settings.hpp"

#include <windows.h>
#include <dpapi.h>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace calcu::desktop {
namespace {
std::wstring encodeToken(const std::wstring& token) {
    if (token.empty()) return {};
    DATA_BLOB plain{static_cast<DWORD>(token.size() * sizeof(wchar_t)),
                    reinterpret_cast<BYTE*>(const_cast<wchar_t*>(token.data()))};
    DATA_BLOB cipher{};
    if (!CryptProtectData(&plain, L"Calcu", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &cipher)) return {};
    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring text;
    for (DWORD i = 0; i < cipher.cbData; ++i) {
        const auto byte = cipher.pbData[i];
        text += hex[byte >> 4];
        text += hex[byte & 15];
    }
    LocalFree(cipher.pbData);
    return text;
}

std::wstring decodeToken(const std::wstring& text) {
    if (text.empty() || text.size() % 2 != 0 || text.size() > 4096) return {};
    std::string bytes;
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const auto hi = std::wstring_view(L"0123456789abcdef").find(text[i]);
        const auto lo = std::wstring_view(L"0123456789abcdef").find(text[i + 1]);
        if (hi > 15 || lo > 15) return {};
        bytes += static_cast<char>((hi << 4) | lo);
    }
    DATA_BLOB cipher{static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(bytes.data())};
    DATA_BLOB plain{};
    if (!CryptUnprotectData(&cipher, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &plain)) return {};
    std::wstring token;
    if (plain.cbData % sizeof(wchar_t) == 0) {
        token.assign(reinterpret_cast<wchar_t*>(plain.pbData), plain.cbData / sizeof(wchar_t));
    }
    SecureZeroMemory(plain.pbData, plain.cbData);
    LocalFree(plain.pbData);
    return validToken(token) ? token : std::wstring{};
}

std::wstring read(const std::wstring& file, const wchar_t* name) {
    wchar_t value[4097]{};
    GetPrivateProfileStringW(L"Calcu", name, L"", value, static_cast<DWORD>(std::size(value)), file.c_str());
    return value;
}
}

std::wstring settingsPath() {
    std::wstring path(32768, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (size == 0 || size >= path.size()) return {};
    path.resize(size);
    return (std::filesystem::path(path).parent_path() / L"calcu.ini").wstring();
}

Settings Settings::load(const std::wstring& file) {
    Settings settings;
    if (file.empty()) return settings;
    const auto combo = read(file, L"shortcut");
    if (combo.size() == 2 && combo[0] <= 127 && combo[1] <= 127) {
        const Shortcut shortcut{static_cast<char>(combo[0]), static_cast<char>(combo[1])};
        if (shortcut.valid()) settings.shortcut = shortcut;
    }
    if (read(file, L"angle") == L"RAD") settings.angle = Angle::radians;
    const auto format = read(file, L"format");
    if (format == L"FRAC") settings.format = ResultFormat::fraction;
    if (format == L"SCI") settings.format = ResultFormat::scientific;
    settings.connection.demo = read(file, L"demo") == L"1";
    const auto url = read(file, L"url");
    if (validUrl(url)) settings.connection.url = url;
    settings.connection.token = decodeToken(read(file, L"token"));
    return settings;
}

bool Settings::save(const std::wstring& file) const {
    if (file.empty()) return false;
    const auto encrypted = encodeToken(connection.token);
    if (!connection.token.empty() && encrypted.empty()) return false;
    const std::wstring combo{static_cast<wchar_t>(shortcut.prefix), static_cast<wchar_t>(shortcut.mode)};
    const std::wstring values = L"\ufeff[Calcu]\r\nshortcut=" + combo +
        L"\r\nangle=" + (angle == Angle::degrees ? L"DEG" : L"RAD") +
        L"\r\nformat=" + (format == ResultFormat::fraction ? L"FRAC" :
            format == ResultFormat::scientific ? L"SCI" : L"DEC") +
        L"\r\ndemo=" + (connection.demo ? L"1" : L"0") +
        L"\r\nurl=" + connection.url + L"\r\ntoken=" + encrypted + L"\r\n";
    const std::wstring temporary = file + L".tmp";
    {
        std::ofstream output(std::filesystem::path(temporary), std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(values.data()),
                     static_cast<std::streamsize>(values.size() * sizeof(wchar_t)));
        output.close();
        if (!output) {
            DeleteFileW(temporary.c_str());
            return false;
        }
    }
    if (MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return true;
    }
    DeleteFileW(temporary.c_str());
    return false;
}

}
