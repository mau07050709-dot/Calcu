#pragma once

#include "calculator.hpp"
#include "chat.hpp"
#include "display.hpp"
#include "keypad.hpp"

#include <array>

namespace calcu {
enum class Mode { calculator, chat };

struct MathEntry {
    std::string expression;
    Evaluation result;
    Angle angle = Angle::degrees;
};

class App {
public:
    App(Display& display, Chat& chat) : display_(display), chat_(chat) {}

    void press(char key, uint32_t now);
    void pressAction(KeyEvent event, uint32_t now);
    bool configureShortcut(Shortcut shortcut);
    Shortcut shortcut() const { return keypad_.shortcut(); }
    void setAngle(Angle angle);
    Angle angle() const { return angle_; }
    void setResultFormat(ResultFormat format);
    ResultFormat resultFormat() const { return resultFormat_; }
    std::string_view expression() const { return expression_; }
    std::size_t cursor() const { return cursor_; }
    void moveMathCursor(std::size_t cursor, uint32_t now);
    const Evaluation* visibleResult() const;
    const MathEntry* historyEntry() const;
    std::size_t historyPosition() const { return historyOffset_ < 0 ? 0 : historyOffset_ + 1; }
    std::size_t historySize() const { return historyCount_; }
    bool showingPalette() const { return paletteOpen_; }
    bool numbers() const { return question_.numbers(); }
    bool showingChatReply() const { return showingChat_; }
    ChatStatus chatStatus() const { return chatStatus_; }
    std::size_t chatHistorySize() const { return chatHistory_.size(); }
    bool readingChatHistory() const { return chatHistoryOffset_ >= 0; }
    void resetConversation();
    void tick(uint32_t now);
    Mode mode() const { return mode_; }

private:
    void handle(KeyEvent event, uint32_t now);
    void calculator(KeyEvent key);
    void conversation(KeyEvent key, uint32_t now);
    void applyReply(ChatReply reply);
    void browseChatHistory();
    void insert(std::string_view token);
    void showMathResult(const Evaluation& result, std::string_view source);
    void showHistory(const MathEntry& entry);
    void remember(const Evaluation& result);
    void navigateHistory(int direction);
    void render();
    Frame calculatorFrame() const;
    Frame chatFrame() const;

    Display& display_;
    Chat& chat_;
    Keypad keypad_;
    Mode mode_ = Mode::calculator;
    Angle angle_ = Angle::degrees;
    MultiTap question_;
    std::string expression_;
    std::size_t cursor_ = 0;
    Pages mathResult_;
    Evaluation currentResult_;
    std::optional<Evaluation> answer_;
    ResultFormat resultFormat_ = ResultFormat::decimal;
    std::array<MathEntry, 8> history_;
    std::size_t nextHistory_ = 0;
    std::size_t historyCount_ = 0;
    int historyOffset_ = -1;
    bool showingMath_ = false;
    bool paletteOpen_ = false;
    std::size_t paletteIndex_ = 0;
    ChatStatus chatStatus_ = ChatStatus::idle;
    Pages chatPages_;
    bool showingChat_ = false;
    ChatHistory chatHistory_;
    int chatHistoryOffset_ = -1;
    Pages transcriptPages_;
    Pages questionPages_;
    bool reviewingQuestion_ = false;
    bool redraw_ = true;
    uint32_t lastDraw_ = 0;
};
}
