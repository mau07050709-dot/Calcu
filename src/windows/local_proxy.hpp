#pragma once

#include "desktop_chat.hpp"
#include <windows.h>

namespace calcu::desktop {
class LocalProxy {
public:
    ~LocalProxy();
    void start(uint32_t now);
    void stop();
    void poll(uint32_t now);
    bool configureKey();
    bool setupFailed();

    const Connection& connection() const { return connection_; }
    const wchar_t* status() const { return status_; }

private:
    HANDLE process_ = nullptr;
    HANDLE input_ = nullptr;
    HANDLE output_ = nullptr;
    HANDLE setup_ = nullptr;
    Connection connection_;
    std::wstring token_;
    std::string bootstrap_;
    uint32_t started_ = 0;
    const wchar_t* status_ = L"Servidor local sin iniciar";
};
}
