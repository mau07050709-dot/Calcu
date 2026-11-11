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

bool validUrl(const std::wstring& url);
bool validToken(const std::wstring& token);

class DesktopChat final : public Chat {
public:
    void configure(Connection connection);
    ChatReply start(std::string_view question, uint32_t now) override;
    ChatReply poll(uint32_t now) override;
    const char* connectionLabel() const override;
    const wchar_t* statusText() const { return status_; }

private:
    Connection connection_;
    std::future<ChatReply> work_;
    std::string demoAnswer_;
    uint32_t started_ = 0;
    const wchar_t* status_ = L"Servidor sin configurar";
};

}
