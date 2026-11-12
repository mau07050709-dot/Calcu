#include "proxy_chat.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
#include <utility>

namespace calcu {
namespace {
constexpr uint32_t uiTimeout = 45000;
constexpr int bodyLimit = 2048;
constexpr auto& net = config::network;

ChatReply error(const char* message) { return {ChatStatus::error, message}; }

class ResponseBuffer : public Stream {
public:
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    std::size_t write(uint8_t value) override { return write(&value, 1); }
    std::size_t write(const uint8_t* data, std::size_t length) override {
        if (length > bodyLimit - size) {
            setWriteError();
            return 0;
        }
        std::memcpy(bytes + size, data, length);
        size += length;
        return length;
    }

    char bytes[bodyLimit]{};
    std::size_t size = 0;
};
}

bool ProxyChat::configured() const {
    return std::strncmp(net.proxyUrl, "https://", 8) == 0 &&
           net.proxyUrl[8] != '\0' && net.rootCa[0] != '\0' && net.deviceToken[0] != '\0';
}

const char* ProxyChat::connectionLabel() const {
    if (!wifi_.ready()) return wifi_.label();
    return configured() ? "WiFi listo" : "Proxy sin config";
}

void ProxyChat::begin() {
    if (!configured()) return;
    requests_ = xQueueCreate(1, sizeof(std::string*));
    results_ = xQueueCreate(1, sizeof(Result));
    if (requests_ && results_ &&
        xTaskCreate(run, "Calcu HTTPS", 12288, this, 1, nullptr) == pdPASS) return;
    if (requests_) vQueueDelete(requests_);
    if (results_) vQueueDelete(results_);
    requests_ = results_ = nullptr;
}

ChatReply ProxyChat::start(std::string_view question, uint32_t now) {
    return startWithHistory(question, {}, now);
}

ChatReply ProxyChat::startWithHistory(std::string_view question, const ChatHistory& history, uint32_t now) {
    if (!configured()) return error("Configura proxy");
    if (!wifi_.ready()) return error(wifi_.label());
    if (!requests_) return error("HTTPS no iniciado");

    // Tras un timeout, descartar el resultado anterior antes de enviar de nuevo.
    if (busy_) {
        Result stale{};
        if (xQueueReceive(results_, &stale, 0) != pdTRUE) return error("Espera a la red");
        busy_ = false;
    }

    if (question.size() > config::questionLimit) return error("Pregunta extensa");
    auto body = std::unique_ptr<std::string>(new (std::nothrow) std::string(chatRequestJson(question, history)));
    if (!body) return error("Memoria escasa");
    auto* request = body.release();
    if (xQueueSend(requests_, &request, 0) != pdTRUE) {
        delete request;
        return error("Espera a la red");
    }

    startedAt_ = now;
    busy_ = true;
    return {ChatStatus::pending, {}};
}

ChatReply ProxyChat::poll(uint32_t now) {
    if (!busy_) return {ChatStatus::idle, {}};
    Result result{};
    if (xQueueReceive(results_, &result, 0) == pdTRUE) {
        busy_ = false;
        return {result.status, result.text};
    }
    if (!wifi_.connected()) return error("WiFi desconectado");
    if (now - startedAt_ >= uiTimeout) return error("Tiempo de espera");
    return {ChatStatus::pending, {}};
}

void ProxyChat::run(void* context) {
    auto& self = *static_cast<ProxyChat*>(context);
    for (;;) {
        std::string* request = nullptr;
        if (xQueueReceive(self.requests_, &request, portMAX_DELAY) != pdTRUE) continue;
        std::unique_ptr<std::string> body(request);
        auto reply = self.ask(*body);
        Result result{};
        result.status = reply.status;
        reply.text.copy(result.text, config::answerLimit);
        xQueueOverwrite(self.results_, &result);
    }
}

ChatReply ProxyChat::ask(std::string& body) const {
    WiFiClientSecure socket;
    socket.setCACert(net.rootCa);
    socket.setHandshakeTimeout(8);
    HTTPClient http;
    http.setConnectTimeout(7000);
    http.setTimeout(32000);
    http.useHTTP10(true);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!http.begin(socket, net.proxyUrl)) return error("URL HTTPS invalida");

    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + net.deviceToken);
    const int status = http.POST(reinterpret_cast<uint8_t*>(body.data()), body.size());
    if (status == 401 || status == 403) return error("Token rechazado");
    if (status == 424) return error("Clave/modelo API");
    if (status == 404) return error("Ruta del proxy");
    if (status == 429) return error("Cuota de API");
    if (status == 503) return error("API sin config.");
    if (status == 504 || status == HTTPC_ERROR_READ_TIMEOUT) return error("Tiempo de espera");
    if (status < 0) return error("Fallo de red/TLS");
    if (status != 200) return error("Error del proxy");

    const int length = http.getSize();
    if (length == 0 || length > bodyLimit) return error("Longitud invalida");
    ResponseBuffer response;
    const int received = http.writeToStream(&response);
    if (received <= 0 || (length >= 0 && received != length) || response.size != static_cast<std::size_t>(received)) {
        return error("Respuesta incompleta");
    }

    JsonDocument output;
    if (deserializeJson(output, response.bytes, response.size)) return error("JSON invalido");
    const char* answer = output["answer"].as<const char*>();
    if (!answer || !*answer) return error("Respuesta vacia");
    std::string text(answer);
    if (text.size() > config::answerLimit) return error("Respuesta extensa");
    for (char& ch : text) if (ch < ' ' || ch > '~') ch = '?';
    return {ChatStatus::answer, std::move(text)};
}
}
