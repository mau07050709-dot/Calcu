#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace calcu {
enum class ChatStatus { idle, pending, answer, error };

struct ChatReply {
    ChatStatus status;
    std::string text;
};

// start/poll/connectionLabel deben regresar inmediatamente.
class Chat {
public:
    virtual ~Chat() = default;
    virtual ChatReply start(std::string_view question, uint32_t now) = 0;
    virtual ChatReply poll(uint32_t now) = 0;
    virtual const char* connectionLabel() const = 0;
};
}
