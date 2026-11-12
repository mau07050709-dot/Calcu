#include "desktop_chat.hpp"
#include "config.hpp"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <type_traits>
#include <utility>

namespace calcu::desktop {
namespace {
struct Endpoint {
    std::wstring host;
    std::wstring path;
    INTERNET_PORT port = 0;
    bool secure = true;
};

Endpoint parseUrl(const std::wstring& url, bool loopback = false) {
    if (url.find(L'#') != std::wstring::npos || url.find(L'\\') != std::wstring::npos ||
        std::any_of(url.begin(), url.end(), [](wchar_t c) { return c <= L' '; })) return {};
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof parts;
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength =
        parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts) ||
        parts.nScheme != (loopback ? INTERNET_SCHEME_HTTP : INTERNET_SCHEME_HTTPS) ||
        parts.dwHostNameLength == 0 ||
        parts.dwUserNameLength != 0 || parts.dwPasswordLength != 0) return {};
    Endpoint result;
    result.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    if (loopback && result.host != L"127.0.0.1") return {};
    result.secure = !loopback;
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
    const auto code = GetLastError();
    if (code == ERROR_WINHTTP_TIMEOUT) return error("Tiempo de espera");
    if (code == ERROR_WINHTTP_SECURE_FAILURE) return error("Certificado TLS");
    if (code == ERROR_WINHTTP_NAME_NOT_RESOLVED) return error("Nombre DNS invalido");
    if (code == ERROR_WINHTTP_CANNOT_CONNECT) return error("Proxy inaccesible");
    return error("Fallo de red");
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

enum class Request { ask, live, health, verify };

ChatReply request(const Connection& connection, std::string body, Request kind) {
    auto endpoint = parseUrl(connection.url, connection.loopback);
    if (endpoint.host.empty()) return error("URL HTTPS invalida");
    if (kind != Request::ask) {
        endpoint.path = endpoint.path.substr(0, endpoint.path.find(L'?'));
        constexpr std::wstring_view route = L"/ask";
        if (endpoint.path.size() < route.size() ||
            endpoint.path.compare(endpoint.path.size() - route.size(), route.size(), route) != 0) {
            return error("La URL debe terminar en /ask");
        }
        endpoint.path.replace(endpoint.path.size() - 3, 3,
            kind == Request::live ? L"live" : kind == Request::health ? L"health" : L"verify");
    }
    InternetHandle session(WinHttpOpen(L"Calcu/2", endpoint.secure ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts(session.value, 5000, 7000, 7000, kind == Request::health || kind == Request::live ? 5000 : 32000)) {
        return networkError();
    }
    InternetHandle host(WinHttpConnect(session.value, endpoint.host.c_str(), endpoint.port, 0));
    if (!host.value) return networkError();
    InternetHandle post(WinHttpOpenRequest(host.value, kind == Request::health || kind == Request::live ? L"GET" : L"POST", endpoint.path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, endpoint.secure ? WINHTTP_FLAG_SECURE : 0));
    if (!post.value) return networkError();
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(post.value, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof redirects)) {
        return networkError();
    }
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (kind != Request::live) headers += L"Authorization: Bearer " + connection.token + L"\r\n";
    const DWORD length = static_cast<DWORD>(body.size());
    if (!WinHttpSendRequest(post.value, headers.c_str(), DWORD(-1), body.data(), length, length, 0) ||
        !WinHttpReceiveResponse(post.value, nullptr)) return networkError();

    DWORD code = 0;
    DWORD codeSize = sizeof code;
    if (!WinHttpQueryHeaders(post.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &code, &codeSize, WINHTTP_NO_HEADER_INDEX)) return networkError();
    if (code == 401 || code == 403) return error("Token rechazado");
    if (code == 424) return error("Clave/modelo API");
    if (code == 503) return error(connection.loopback && kind == Request::ask ? "Clave API falta" :
        "Revisa clave y modelo en el servidor.");
    if (code == 429) return error("Revisa cuota o limite de OpenAI.");
    if (code == 504) return error("Tiempo de espera");
    if (code == 404) return error("Ruta del proxy");
    if (code != 200) return error("Error del proxy");

    std::string json;
    char buffer[512];
    DWORD count = 0;
    do {
        if (!WinHttpReadData(post.value, buffer, sizeof buffer, &count)) return networkError();
        if (json.size() + count > 2048) return error("Respuesta extensa");
        json.append(buffer, count);
    } while (count != 0);
    if (kind != Request::ask) {
        const char* expected = kind == Request::live ? R"({"service":"calcu","available":true})" :
            kind == Request::health ? R"({"service":"calcu","ready":true})" :
            R"({"service":"calcu","openai":true})";
        if (json != expected) return error("El servidor no devolvio el estado de Calcu.");
        return {ChatStatus::answer, {}};
    }
    auto answer = answerFromJson(json);
    if (answer.empty()) return error("Respuesta invalida");
    return {ChatStatus::answer, std::move(answer)};
}

ConnectionCheck diagnose(const Connection& connection) {
    unsigned completed = 0;
    for (const auto kind : {Request::live, Request::health, Request::verify}) {
        if (kind == Request::health && !validToken(connection.token)) {
            return {ChatStatus::error, completed, "Pega CALCU_DEVICE_TOKEN del servidor."};
        }
        const auto result = request(connection, {}, kind);
        if (result.status != ChatStatus::answer) {
            std::string detail = result.text;
            if (detail == "Clave/modelo API" || (kind == Request::verify && detail == "Revisa clave y modelo en el servidor.")) {
                detail = "Revisa OPENAI_API_KEY y OPENAI_MODEL en el servidor.";
            } else if (detail == "Token rechazado") {
                detail = "Usa el mismo CALCU_DEVICE_TOKEN del servidor.";
            } else if (detail == "Certificado TLS") {
                detail = "Revisa el nombre y la confianza del certificado TLS.";
            } else if (detail == "Ruta del proxy") {
                detail = "Comprueba la URL /ask y actualiza el servidor a Fase 5.";
            }
            return {ChatStatus::error, completed, std::move(detail)};
        }
        ++completed;
    }
    return {ChatStatus::answer, completed, {}};
}

template<class T>
bool ready(const std::future<T>& job) {
    return job.valid() && job.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

template<class Work>
auto launch(Work work) {
    std::packaged_task<std::invoke_result_t<Work>()> job(std::move(work));
    auto result = job.get_future();
    std::thread(std::move(job)).detach();
    return result;
}

ChatReply take(std::future<ChatReply>& job) {
    try { return job.get(); }
    catch (const std::exception&) { return error("Fallo de red"); }
}
}

bool validUrl(const std::wstring& url) { return !parseUrl(url).host.empty(); }

bool validToken(const std::wstring& token) {
    return token.size() >= 32 && token.size() <= 256 &&
        std::all_of(token.begin(), token.end(), [](wchar_t c) { return c >= L'!' && c <= L'~'; });
}

void DesktopChat::configure(Connection connection) {
    connection_ = std::move(connection);
    localSetupNeeded_ = false;
    status_ = connection_.demo ? L"Demostración local" : connection_.loopback ? L"Servidor local listo" :
        connection_.configured() ? L"HTTPS configurado" : L"Servidor sin configurar";
}

ChatReply DesktopChat::start(std::string_view question, uint32_t now) {
    return startWithHistory(question, {}, now);
}

ChatReply DesktopChat::startWithHistory(std::string_view question, const ChatHistory& history, uint32_t now) {
    if (connection_.demo) {
        started_ = now;
        demoAnswer_ = "Modo demostracion. Pregunta: " + std::string(question) +
            ". Esta respuesta permite probar la pantalla de 16 por 2. "
            "Avanza y retrocede por las paginas para leer el contenido completo.";
        return {ChatStatus::pending, {}};
    }
    if (!connection_.configured()) return error(connection_.loopback ? "Servidor local" : "Configura proxy");
    if (work_.valid()) {
        if (work_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return error("Espera a la red");
        }
        work_ = {};
    }
    try {
        work_ = launch([connection = connection_, body = chatRequestJson(question, history)]() mutable {
            return request(connection, std::move(body), Request::ask);
        });
    } catch (const std::exception&) {
        return error("Fallo de inicio");
    }
    status_ = connection_.loopback ? L"Consultando servidor local…" : L"Conectando por HTTPS…";
    started_ = now;
    return {ChatStatus::pending, {}};
}

ChatReply DesktopChat::poll(uint32_t now) {
    if (!demoAnswer_.empty()) {
        if (now - started_ < 300) return {ChatStatus::pending, {}};
        return {ChatStatus::answer, std::exchange(demoAnswer_, {})};
    }
    if (!work_.valid()) return {ChatStatus::idle, {}};
    if (ready(work_)) {
        auto reply = take(work_);
        localSetupNeeded_ = connection_.loopback && reply.status == ChatStatus::error && reply.text == "Clave API falta";
        status_ = reply.status == ChatStatus::answer ? L"Respuesta recibida" :
            localSetupNeeded_ ? L"Configura la clave API de OpenAI" : L"Error de conexión";
        return reply;
    }
    if (now - started_ >= 45000) {
        status_ = L"Tiempo de espera agotado";
        return error("Tiempo de espera");
    }
    return {ChatStatus::pending, {}};
}

ConnectionCheck DesktopChat::check(const Connection& connection, uint32_t now) {
    if (parseUrl(connection.url, connection.loopback).host.empty()) {
        return {ChatStatus::error, 0, "Escribe la URL HTTPS terminada en /ask."};
    }
    if (check_.valid() && !ready(check_)) return {ChatStatus::error, 0, "La prueba anterior sigue en curso."};
    try { check_ = launch([connection] { return diagnose(connection); }); }
    catch (const std::exception&) { return {ChatStatus::error, 0, "No se pudo iniciar la prueba."}; }
    checkStarted_ = now;
    return {ChatStatus::pending, 0, {}};
}

ConnectionCheck DesktopChat::pollCheck(uint32_t now) {
    if (!check_.valid()) return {ChatStatus::idle, 0, {}};
    if (ready(check_)) {
        try { return check_.get(); }
        catch (const std::exception&) { return {ChatStatus::error, 0, "Fallo de red."}; }
    }
    if (now - checkStarted_ >= 55000) return {ChatStatus::error, 0, "Tiempo de espera de la prueba."};
    return {ChatStatus::pending, 0, {}};
}

bool DesktopChat::takeLocalSetupNeeded() {
    return std::exchange(localSetupNeeded_, false);
}

const char* DesktopChat::connectionLabel() const {
    if (connection_.demo) return "Demostracion";
    if (connection_.loopback) return "Servidor local";
    return connection_.configured() ? "HTTPS configurado" : "Sin servidor";
}

}
