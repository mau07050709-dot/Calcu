#include "chat.hpp"

namespace calcu {
namespace {
void appendJsonString(std::string& json, std::string_view text) {
    json += '"';
    for (char c : text) {
        if (c == '"' || c == '\\') json += '\\';
        json += c >= ' ' && c <= '~' ? c : '?';
    }
    json += '"';
}
}

std::string chatRequestJson(std::string_view question, const ChatHistory& history) {
    std::string json = "{\"question\":";
    appendJsonString(json, question);
    json += ",\"history\":[";
    bool first = true;
    for (const auto& turn : history) {
        if (!first) json += ',';
        first = false;
        json += "{\"question\":";
        appendJsonString(json, turn.question);
        json += ",\"answer\":";
        appendJsonString(json, turn.answer);
        json += '}';
    }
    json += "]}";
    return json;
}
}
