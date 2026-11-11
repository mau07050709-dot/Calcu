#pragma once

#include "chat.hpp"
#include "config.hpp"
#include "wifi.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace calcu {
class ProxyChat : public Chat {
public:
    explicit ProxyChat(Wifi& wifi) : wifi_(wifi) {}
    void begin();
    ChatReply start(std::string_view question, uint32_t now) override;
    ChatReply poll(uint32_t now) override;
    const char* connectionLabel() const override;

private:
    struct Request { char question[config::questionLimit + 1]; };
    struct Result {
        ChatStatus status;
        char text[config::answerLimit + 1];
    };

    static void run(void* context);
    ChatReply ask(const char* question) const;
    bool configured() const;

    Wifi& wifi_;
    QueueHandle_t requests_ = nullptr;
    QueueHandle_t results_ = nullptr;
    uint32_t startedAt_ = 0;
    bool busy_ = false;
};
}
