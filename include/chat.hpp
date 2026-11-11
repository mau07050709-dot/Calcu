#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace calcu {
enum class ChatStatus { idle, pending, answer, error };

struct ChatReply {
    ChatStatus status;
    std::string text;
};

struct ChatTurn {
    std::string question;
    std::string answer;
};
using ChatHistory = std::vector<ChatTurn>;

std::string chatRequestJson(std::string_view question, const ChatHistory& history);

// start/poll/connectionLabel deben regresar inmediatamente.
class Chat {
public:
    virtual ~Chat() = default;
    virtual ChatReply start(std::string_view question, uint32_t now) = 0;
    virtual ChatReply startWithHistory(std::string_view question, const ChatHistory&, uint32_t now) {
        return start(question, now);
    }
    virtual ChatReply poll(uint32_t now) = 0;
    virtual const char* connectionLabel() const = 0;
};
}
