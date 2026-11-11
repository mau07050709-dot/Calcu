#include "desktop_chat.hpp"
#include "config.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>

namespace calcu::desktop {
namespace {
struct Endpoint {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
};

Endpoint parseUrl(const std::wstring& url) {
    if (url.find(L'#') != std::wstring::npos || url.find(L'\\') != std::wstring::npos ||
        std::any_of(url.begin(), url.end(), [](wchar_t c) { return c <= L' '; })) return {};
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof parts;
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
        parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS || parts.dwHostNameLength == 0 ||
        parts.dwUserNameLength != 0 || parts.dwPasswordLength != 0) return {};
    Endpoint result;
    result.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    if (parts.dwUrlPathLength) result.path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (result.path.empty()) result.path = L"/";
    if (parts.dwExtraInfoLength) result.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    result.port = parts.nPort;
    return result;
}

struct InternetHandle {
    explicit InternetHandle(HINTERNET handle) : value(handle) {}
    InternetHandle(const InternetHandle&) = delete;
    InternetHandle& operator=(const InternetHandle&) = delete;
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
    HINTERNET value;
};

ChatReply error(const char* message) { return {ChatStatus::error, message}; }

ChatReply networkError() {
    if (GetLastError() == ERROR_WINHTTP_TIMEOUT) return error("Tiempo de espera");
    if (GetLastError() == ERROR_WINHTTP_SECURE_FAILURE) return error("Certificado TLS");
    return error("Fallo de red");
}

std::string questionJson(std::string_view question) {
    std::string json = "{\"question\":\"";
    for (char c : question) {
        if (c == '\\' || c == '"') json += '\\';
        json += c >= ' ' && c <= '~' ? c : '?';
    }
    return json + "\"}";
}

// El contrato del intermediario devuelve un objeto con un solo campo ASCII.
std::string answerFromJson(std::string_view json) {
    auto trim = [&] {
        const auto position = json.find_first_not_of(" \r\n\t");
        if (position == std::string_view::npos) json = {};
        else json.remove_prefix(position);
    };
    auto take = [&](std::string_view token) {
        trim();
        if (json.substr(0, token.size()) != token) return false;
        json.remove_prefix(token.size());
        return true;
    };
    if (!take("{") || !take("\"answer\"") || !take(":") || !take("\"")) return {};
    std::string answer;
    while (!json.empty()) {
        char c = json.front();
        json.remove_prefix(1);
        if (c == '"') {
            if (!take("}")) return {};
            trim();
            return json.empty() ? answer : std::string{};
        }
        if (c < ' ' || c > '~') return {};
        if (c == '\\') {
            if (json.empty()) return {};
            c = json.front();
            json.remove_prefix(1);
            if (c == 'n' || c == 'r' || c == 't' || c == 'b' || c == 'f') c = ' ';
            else if (c != '\\' && c != '"' && c != '/') return {};
        }
        if (answer.size() == config::answerLimit) return {};
        answer += c;
    }
    return {};
}

ChatReply request(Connection connection, std::string question) {
    const auto endpoint = parseUrl(connection.url);
    if (endpoint.host.empty()) return error("URL HTTPS invalida");
    InternetHandle session(WinHttpOpen(L"Calcu/2", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts(session.value, 5000, 7000, 7000, 32000)) {
        return networkError();
    }
    InternetHandle host(WinHttpConnect(session.value, endpoint.host.c_str(), endpoint.port, 0));
    if (!host.value) return networkError();
    InternetHandle post(WinHttpOpenRequest(host.value, L"POST", endpoint.path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!post.value) return networkError();
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(post.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof redirects)) {
        return networkError();
    }
    auto body = questionJson(question);
    const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " +
        connection.token + L"\r\n";
    const DWORD length = static_cast<DWORD>(body.size());
    if (!WinHttpSendRequest(post.value, headers.c_str(), DWORD(-1), body.data(), length, length, 0) ||
        !WinHttpReceiveResponse(post.value, nullptr)) return networkError();

    DWORD code = 0;
    DWORD codeSize = sizeof code;
    if (!WinHttpQueryHeaders(post.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeSize, WINHTTP_NO_HEADER_INDEX)) return networkError();
    if (code == 401 || code == 403) return error("Token rechazado");
    if (code == 429 || code == 503) return error("Servidor ocupado");
    if (code == 504) return error("Tiempo de espera");
    if (code != 200) return error("Error del proxy");

    std::string json;
    char buffer[512];
    DWORD count = 0;
    do {
        if (!WinHttpReadData(post.value, buffer, sizeof buffer, &count)) return networkError();
        if (json.size() + count > 2048) return error("Respuesta extensa");
        json.append(buffer, count);
    } while (count != 0);
    auto answer = answerFromJson(json);
    if (answer.empty()) return error("Respuesta invalida");
    return {ChatStatus::answer, std::move(answer)};
}
}

bool validUrl(const std::wstring& url) { return !parseUrl(url).host.empty(); }

bool validToken(const std::wstring& token) {
    return token.size() >= 32 && token.size() <= 256 &&
        std::all_of(token.begin(), token.end(), [](wchar_t c) { return c >= L'!' && c <= L'~'; });
}

void DesktopChat::configure(Connection connection) {
    connection_ = std::move(connection);
    status_ = connection_.demo ? L"Demostración local" :
        connection_.configured() ? L"HTTPS configurado" : L"Servidor sin configurar";
}

ChatReply DesktopChat::start(std::string_view question, uint32_t now) {
    if (connection_.demo) {
        started_ = now;
        demoAnswer_ = "Modo demostracion. Pregunta: " + std::string(question) +
            ". Esta respuesta permite probar la pantalla de 16 por 2. "
            "Avanza y retrocede por las paginas para leer el contenido completo.";
        return {ChatStatus::pending, {}};
    }
    if (!connection_.configured()) return error("Configura proxy");
    if (work_.valid()) {
        if (work_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return error("Espera a la red");
        }
        work_ = {};
    }
    try {
        std::packaged_task<ChatReply(Connection, std::string)> job(request);
        auto result = job.get_future();
        std::thread(std::move(job), connection_, std::string(question)).detach();
        work_ = std::move(result);
    } catch (const std::exception&) {
        return error("Fallo de inicio");
    }
    status_ = L"Conectando por HTTPS…";
    started_ = now;
    return {ChatStatus::pending, {}};
}

ChatReply DesktopChat::poll(uint32_t now) {
    if (!demoAnswer_.empty()) {
        if (now - started_ < 300) return {ChatStatus::pending, {}};
        return {ChatStatus::answer, std::exchange(demoAnswer_, {})};
    }
    if (!work_.valid()) return {ChatStatus::idle, {}};
    if (work_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        ChatReply reply;
        try { reply = work_.get(); }
        catch (const std::exception&) { reply = error("Fallo de red"); }
        status_ = reply.status == ChatStatus::answer ? L"HTTPS conectado" : L"Error de conexión";
        return reply;
    }
    if (now - started_ >= 45000) {
        status_ = L"Tiempo de espera agotado";
        return error("Tiempo de espera");
    }
    return {ChatStatus::pending, {}};
}

const char* DesktopChat::connectionLabel() const {
    if (connection_.demo) return "Demostracion";
    return connection_.configured() ? "HTTPS configurado" : "Sin servidor";
}

}
