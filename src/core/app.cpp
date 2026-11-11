#include "app.hpp"
#include "config.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace calcu {
namespace {
constexpr std::array<std::string_view, 29> tokens{
    "+", "-", "*", "/", ".", "^", "%", "(", ")", "sqrt(", "cbrt(",
    "sin(", "cos(", "tan(", "log(", "ln(", "pi", "e",
    "root(", "asin(", "acos(", "atan(", "ncr(", "npr(", "!", ",", "E", "Ans", "logb("
};

constexpr std::size_t paletteSize = tokens.size() + 2;

Frame editor(std::string title, std::string text, std::size_t cursor) {
    text.insert(cursor, 1, '|');
    const auto start = cursor >= lcdWidth ? cursor - lcdWidth + 1 : 0;
    return {std::move(title), text.substr(start, lcdWidth)};
}
}

void App::press(char key, uint32_t now) {
    handle(keypad_.press(key, now), now);
}

void App::pressAction(KeyEvent event, uint32_t now) {
    keypad_.cancelShortcut();
    handle(event, now);
}

bool App::configureShortcut(Shortcut shortcut) {
    redraw_ = true;
    return keypad_.configure(shortcut);
}

void App::setAngle(Angle angle) {
    angle_ = angle;
    showingMath_ = false;
    redraw_ = true;
}

void App::setResultFormat(ResultFormat format) {
    resultFormat_ = format;
    if (const auto* entry = historyEntry()) showHistory(*entry);
    else if (showingMath_) showMathResult(currentResult_, expression_);
    redraw_ = true;
}

void App::moveMathCursor(std::size_t cursor, uint32_t now) {
    if (mode_ != Mode::calculator || historyEntry()) return;
    cursor_ = std::min(cursor, expression_.size());
    paletteOpen_ = false;
    showingMath_ = false;
    keypad_.cancelShortcut();
    redraw_ = true;
    tick(now);
}

const MathEntry* App::historyEntry() const {
    if (historyOffset_ < 0) return nullptr;
    const auto index = (nextHistory_ + history_.size() - 1 - historyOffset_) % history_.size();
    return &history_[index];
}

const Evaluation* App::visibleResult() const {
    if (const auto* entry = historyEntry()) return &entry->result;
    return showingMath_ ? &currentResult_ : nullptr;
}

void App::showMathResult(const Evaluation& result, std::string_view source) {
    auto text = formatResult(result, resultFormat_);
    text.resize(((text.size() + lcdWidth - 1) / lcdWidth) * lcdWidth, ' ');
    text.append(source);
    mathResult_.set(text);
}

void App::showHistory(const MathEntry& entry) {
    std::string heading = "Hist " + std::to_string(historyPosition()) + "/" + std::to_string(historySize());
    heading.resize(lcdWidth, ' ');
    auto result = formatResult(entry.result, resultFormat_);
    result.resize(((result.size() + lcdWidth - 1) / lcdWidth) * lcdWidth, ' ');
    mathResult_.set(heading + result + entry.expression);
}

void App::remember(const Evaluation& result) {
    answer_ = result;
    history_[nextHistory_] = {expression_, result, angle_};
    nextHistory_ = (nextHistory_ + 1) % history_.size();
    historyCount_ = std::min(historyCount_ + 1, history_.size());
}

void App::navigateHistory(int direction) {
    if (historyCount_ == 0) return;
    historyOffset_ = std::clamp(historyOffset_ + direction, -1, static_cast<int>(historyCount_) - 1);
    if (const auto* entry = historyEntry()) showHistory(*entry);
    else if (showingMath_) showMathResult(currentResult_, expression_);
}

void App::handle(KeyEvent event, uint32_t now) {
    if (event.action == Action::mode) {
        question_.commit();
        mode_ = mode_ == Mode::calculator ? Mode::chat : Mode::calculator;
    } else if (mode_ == Mode::calculator) {
        calculator(event);
    } else {
        conversation(event, now);
    }
    redraw_ = true;
    tick(now);
}

void App::tick(uint32_t now) {
    const bool hadShortcut = keypad_.shortcutPending();
    keypad_.tick(now);
    if (hadShortcut != keypad_.shortcutPending()) redraw_ = true;
    question_.tick(now);
    if (chatStatus_ == ChatStatus::pending) applyReply(chat_.poll(now));
    if (redraw_ || (mode_ == Mode::chat && now - lastDraw_ >= 250)) {
        render();
        redraw_ = false;
        lastDraw_ = now;
    }
}

void App::insert(std::string_view token) {
    if (expression_.size() + token.size() > config::expressionLimit) return;
    expression_.insert(cursor_, token);
    cursor_ += token.size();
}

void App::calculator(KeyEvent key) {
    if (key.action == Action::resultFormat) {
        const auto next = (static_cast<int>(resultFormat_) + 1) % 3;
        setResultFormat(static_cast<ResultFormat>(next));
        return;
    }
    if (key.action == Action::historyOlder || key.action == Action::historyNewer) {
        paletteOpen_ = false;
        navigateHistory(key.action == Action::historyOlder ? 1 : -1);
        return;
    }
    if (const auto* entry = historyEntry()) {
        switch (key.action) {
            case Action::enter:
                expression_ = entry->expression;
                cursor_ = expression_.size();
                angle_ = entry->angle;
                historyOffset_ = -1;
                showingMath_ = false;
                return;
            case Action::left: navigateHistory(1); return;
            case Action::right: navigateHistory(-1); return;
            case Action::erase: navigateHistory(-static_cast<int>(historyCount_)); return;
            case Action::cursorLeft: mathResult_.move(-1); return;
            case Action::cursorRight: mathResult_.move(1); return;
            case Action::none: return;
            default:
                historyOffset_ = -1;
                showingMath_ = false;
                break;
        }
    }
    if (key.action == Action::symbol) {
        if (std::find(tokens.begin(), tokens.end(), key.symbol) == tokens.end()) return;
        paletteOpen_ = false;
        showingMath_ = false;
        insert(key.symbol);
        return;
    }
    if (paletteOpen_) {
        switch (key.action) {
            case Action::left: paletteIndex_ = (paletteIndex_ + paletteSize - 1) % paletteSize; break;
            case Action::right: paletteIndex_ = (paletteIndex_ + 1) % paletteSize; break;
            case Action::enter:
                paletteOpen_ = false;
                if (paletteIndex_ < tokens.size()) insert(tokens[paletteIndex_]);
                else calculator({paletteIndex_ == tokens.size() ? Action::resultFormat : Action::historyOlder});
                break;
            case Action::erase:
            case Action::paletteOrNumbers: paletteOpen_ = false; break;
            case Action::clear:
                paletteOpen_ = false;
                expression_.clear();
                cursor_ = 0;
                break;
            default: break;
        }
        return;
    }

    if (showingMath_) {
        if (key.action == Action::left || key.action == Action::right) {
            mathResult_.move(key.action == Action::left ? -1 : 1);
            return;
        }
        if (key.action == Action::cursorLeft || key.action == Action::cursorRight) {
            showingMath_ = false;
        } else if (key.action == Action::erase) {
            showingMath_ = false;
            return;
        }
        if (key.action == Action::none) return;
        showingMath_ = false;
    }
    switch (key.action) {
        case Action::digit: insert(std::string_view(&key.digit, 1)); break;
        case Action::paletteOrNumbers: paletteOpen_ = true; break;
        case Action::cursorLeft:
        case Action::left: if (cursor_ > 0) --cursor_; break;
        case Action::cursorRight:
        case Action::right: if (cursor_ < expression_.size()) ++cursor_; break;
        case Action::erase:
            if (cursor_ > 0) expression_.erase(--cursor_, 1);
            break;
        case Action::clear: expression_.clear(); cursor_ = 0; break;
        case Action::angleOrCommit:
            angle_ = angle_ == Angle::degrees ? Angle::radians : Angle::degrees;
            break;
        case Action::enter: {
            currentResult_ = evaluate(expression_, angle_, answer_ ? &*answer_ : nullptr);
            if (currentResult_.error == MathError::none) remember(currentResult_);
            showMathResult(currentResult_, expression_);
            showingMath_ = true;
            break;
        }
        default: break;
    }
}

void App::resetConversation() {
    if (chatStatus_ == ChatStatus::pending) return;
    chatHistory_.clear();
    chatHistoryOffset_ = -1;
    transcriptPages_.set({});
    chatPages_.set({});
    chatStatus_ = ChatStatus::idle;
    showingChat_ = false;
    redraw_ = true;
}

void App::browseChatHistory() {
    if (chatHistory_.empty()) return;
    chatHistoryOffset_ = (chatHistoryOffset_ + 1) % static_cast<int>(chatHistory_.size());
    const auto& turn = chatHistory_[chatHistory_.size() - 1 - chatHistoryOffset_];
    auto heading = "Hist " + std::to_string(chatHistoryOffset_ + 1) + "/" +
        std::to_string(chatHistory_.size());
    heading.resize(lcdWidth, ' ');
    auto question = "TU: " + turn.question;
    question.resize(((question.size() + lcdWidth - 1) / lcdWidth) * lcdWidth, ' ');
    transcriptPages_.set(heading + question + "GPT: " + turn.answer);
}

void App::applyReply(ChatReply reply) {
    if (reply.status == ChatStatus::pending || reply.status == ChatStatus::idle) return;
    chatStatus_ = reply.status;
    showingChat_ = true;
    redraw_ = true;
    if (reply.status == ChatStatus::error) {
        std::string heading = "Error ChatGPT";
        heading.resize(lcdWidth, ' ');
        chatPages_.set(heading + reply.text);
    } else {
        reply.text.resize(std::min(reply.text.size(), config::answerLimit));
        const auto& draft = question_.text();
        const auto first = draft.find_first_not_of(' ');
        if (first != std::string::npos) {
            if (chatHistory_.size() == config::chatHistoryLimit) chatHistory_.erase(chatHistory_.begin());
            chatHistory_.push_back({draft.substr(first, draft.find_last_not_of(' ') - first + 1), reply.text});
        }
        question_.clear();
        chatPages_.set(reply.text);
    }
}

void App::conversation(KeyEvent key, uint32_t now) {
    if (chatStatus_ == ChatStatus::pending || key.action == Action::none) return;
    if (key.action == Action::chatHistory) {
        browseChatHistory();
        return;
    }
    if (readingChatHistory()) {
        if (key.action == Action::left || key.action == Action::right) {
            transcriptPages_.move(key.action == Action::left ? -1 : 1);
            return;
        }
        chatHistoryOffset_ = -1;
        if (key.action == Action::enter || key.action == Action::erase) return;
    }

    if (showingChat_) {
        if (key.action == Action::left || key.action == Action::right) {
            chatPages_.move(key.action == Action::left ? -1 : 1);
            return;
        }
        showingChat_ = false;
        if (key.action == Action::erase || key.action == Action::enter) return;
    }
    if (key.action == Action::left || key.action == Action::right) {
        if (question_.text().size() < lcdWidth) return;
        question_.commit();
        if (!reviewingQuestion_) {
            questionPages_.set(question_.text());
            reviewingQuestion_ = true;
        } else {
            questionPages_.move(key.action == Action::left ? -1 : 1);
        }
        return;
    }
    reviewingQuestion_ = false;
    switch (key.action) {
        case Action::digit: question_.press(key.digit, now); break;
        case Action::paletteOrNumbers: question_.toggleNumbers(); break;
        case Action::cursorLeft: question_.move(-1); break;
        case Action::cursorRight: question_.move(1); break;
        case Action::erase: question_.erase(); break;
        case Action::clear: question_.clear(); break;
        case Action::angleOrCommit: question_.commit(); break;
        case Action::enter:
            question_.commit();
            if (question_.text().find_first_not_of(' ') != std::string::npos) {
                auto reply = chat_.startWithHistory(question_.text(), chatHistory_, now);
                if (reply.status == ChatStatus::pending) chatStatus_ = ChatStatus::pending;
                applyReply(std::move(reply));
            }
            break;
        default: break;
    }
}

Frame App::calculatorFrame() const {
    if (paletteOpen_) {
        if (paletteIndex_ < tokens.size()) return {"Insertar simbolo", "[" + std::string(tokens[paletteIndex_]) + "]"};
        return {"Calculadora", paletteIndex_ == tokens.size() ? "[Formato]" : "[Historial]"};
    }
    if (visibleResult()) return mathResult_.frame();
    return editor(angle_ == Angle::degrees ? "Calcu DEG" : "Calcu RAD", expression_, cursor_);
}

Frame App::chatFrame() const {
    if (chatStatus_ == ChatStatus::pending) return {"ChatGPT", "Consultando..."};
    if (readingChatHistory()) return transcriptPages_.frame();
    if (showingChat_) return chatPages_.frame();
    if (reviewingQuestion_) return questionPages_.frame();
    return editor(question_.numbers() ? "ChatGPT 123" : "ChatGPT ABC",
                  question_.text(), question_.cursor());
}

void App::render() {
    auto frame = mode_ == Mode::calculator ? calculatorFrame() : chatFrame();
    if (mode_ == Mode::chat && chatStatus_ != ChatStatus::pending &&
        !showingChat_ && !readingChatHistory() && question_.text().empty()) {
        frame[1] = chat_.connectionLabel();
    }
    if (keypad_.shortcutPending()) {
        frame[0].resize(lcdWidth, ' ');
        frame[0].back() = keypad_.shortcut().prefix;
    }
    display_.show(std::move(frame));
}
}
