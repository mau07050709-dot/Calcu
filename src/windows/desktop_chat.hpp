#pragma once

#include "chat.hpp"

#include <future>

namespace calcu::desktop {

struct Connection {
    bool demo = false;
    std::wstring url;
    std::wstring token;

    bool configured() const { return !url.empty() && !token.empty(); }
};

struct ConnectionCheck {
    ChatStatus status;
    unsigned completed = 0;
    std::string detail;
};

bool validUrl(const std::wstring& url);
bool validToken(const std::wstring& token);

class DesktopChat final : public Chat {
public:
    void configure(Connection connection);
    ChatReply start(std::string_view question, uint32_t now) override;
    ChatReply startWithHistory(std::string_view question, const ChatHistory& history, uint32_t now) override;
    ChatReply poll(uint32_t now) override;
    ConnectionCheck check(const Connection& connection, uint32_t now);
    ConnectionCheck pollCheck(uint32_t now);
    const char* connectionLabel() const override;
    const wchar_t* statusText() const { return status_; }

private:
    Connection connection_;
    std::future<ChatReply> work_;
    std::future<ConnectionCheck> check_;
    uint32_t checkStarted_ = 0;
    std::string demoAnswer_;
    uint32_t started_ = 0;
    const wchar_t* status_ = L"Servidor sin configurar";
};

}
