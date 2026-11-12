#include "app.hpp"
#include "desktop_chat.hpp"
#include "local_proxy.hpp"
#include "settings.hpp"
#include "scientific_display.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {
using namespace calcu;
using namespace calcu::desktop;

constexpr int width = 718;
constexpr int height = 677;
constexpr COLORREF background = RGB(18, 25, 33);
constexpr COLORREF lcdBackground = RGB(199, 223, 168);
constexpr COLORREF lcdInk = RGB(36, 59, 30);
constexpr COLORREF ink = RGB(238, 243, 249);
constexpr COLORREF muted = RGB(162, 180, 200);

constexpr int digitBase = 100;
constexpr int symbolBase = 200;
enum Id {
    star = 300, hash, switchMode, preferences, erase, clear, cursorLeft, cursorRight,
    go, angle, letters, commit, previous, next, save, cancel,
    prefixField, modeField, urlField, tokenField, demoField, formatKey, historyOlder, historyNewer,
    testConnection, conversationHistory, setupOpenAI, setupOpenAISettings
};
enum class Panel { common, calculator, chat, settings };
enum Style { neutral, primary, mutedButton };

struct Symbol {
    const wchar_t* label;
    std::string_view token;
};

constexpr std::array<Symbol, 28> symbols{{
    {L"+", "+"}, {L"−", "-"}, {L"×", "*"}, {L"a⁄b", "/"}, {L"xʸ", "^"},
    {L"(", "("}, {L")", ")"}, {L".", "."}, {L"%", "%"}, {L"x!", "!"},
    {L"√", "sqrt("}, {L"∛", "cbrt("}, {L"ⁿ√", "root("}, {L",", ","}, {L"×10ⁿ", "E"},
    {L"sin", "sin("}, {L"cos", "cos("}, {L"tan", "tan("}, {L"sin⁻¹", "asin("}, {L"cos⁻¹", "acos("},
    {L"tan⁻¹", "atan("}, {L"log", "log("}, {L"logₐ", "logb("}, {L"ln", "ln("}, {L"π", "pi"},
    {L"e", "e"}, {L"nCr", "ncr("}, {L"nPr", "npr("}
}};
const wchar_t* formatLabel(ResultFormat format) {
    if (format == ResultFormat::fraction) return L"Fracción";
    if (format == ResultFormat::scientific) return L"SCI";
    return L"Decimal";
}
constexpr std::array<const wchar_t*, 10> abc{
    L"0  espacio", L"1  . , ?", L"2  ABC", L"3  DEF", L"4  GHI", L"5  JKL",
    L"6  MNO", L"7  PQRS", L"8  TUV", L"9  WXYZ"
};

std::wstring textOf(HWND control) {
    std::wstring value(static_cast<std::size_t>(GetWindowTextLengthW(control)) + 1, L'\0');
    value.resize(GetWindowTextW(control, value.data(), static_cast<int>(value.size())));
    return value;
}

void textIfChanged(HWND control, const std::wstring& value) {
    if (textOf(control) != value) SetWindowTextW(control, value.c_str());
}

class Lcd final : public Display {
public:
    HWND rows[2]{};
private:
    void write(const Frame& frame, const Frame& previous) override {
        for (std::size_t i = 0; i < frame.size(); ++i) {
            if (frame[i] == previous[i]) continue;
            const std::wstring line(frame[i].begin(), frame[i].end());
            SetWindowTextW(rows[i], line.c_str());
        }
    }
};

class Window {
public:
    ~Window() {
        for (auto font : {regular_, bold_, logo_, mono_}) if (font) DeleteObject(font);
        if (backgroundBrush_) DeleteObject(backgroundBrush_);
        if (lcdBrush_) DeleteObject(lcdBrush_);
    }

    bool create(HINSTANCE instance);
    bool key(const MSG& message);
    HWND handle() const { return window_; }

private:
    struct Control { HWND window; Panel panel; };
    struct Fonts {
        static HFONT make(int size, int weight, const wchar_t* name) {
            return CreateFontW(-size, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, name);
        }
    };

    static LRESULT CALLBACK dispatch(HWND window, UINT message, WPARAM w, LPARAM l);
    LRESULT process(UINT message, WPARAM w, LPARAM l);
    int px(int value) const { return MulDiv(value, dpi_, 96); }
    HWND control(const wchar_t* type, const wchar_t* label, DWORD style, int id,
                 int x, int y, int w, int h, Panel panel, HFONT font = nullptr);
    HWND label(const wchar_t* text, int x, int y, int w, int h, Panel panel, HFONT font = nullptr);
    void button(int id, const wchar_t* label, int x, int y, int w, int h, Panel panel,
                Style style = neutral);
    void build();
    void drawButton(const DRAWITEMSTRUCT& item);
    void paint();
    void refresh();
    void showSettings();
    void storeSettings();
    Connection connectionFromFields() const;
    bool usesLocalProxy() const;
    void applyConnection();
    void updateLocalProxy();
    void setupOpenAIKey();
    void checkConnection();
    void updateConnectionCheck();
    void showConnectionCheck(const ConnectionCheck& result);
    void notify(const wchar_t* value);
    void persistMathSettings();
    RECT mathInput() const { return {px(49), px(118), px(669), px(202)}; }
    RECT mathOutput() const { return {px(49), px(207), px(669), px(270)}; }
    void command(int id);
    void action(Action action, std::string_view symbol = {});
    void raw(char key);

    HWND window_ = nullptr;
    HWND heading_ = nullptr;
    HWND status_ = nullptr;
    HWND hint_ = nullptr;
    HWND modeButton_ = nullptr;
    HWND angleButton_ = nullptr;
    HWND lettersButton_ = nullptr;
    HWND sendButton_ = nullptr;
    HWND settingsStatus_ = nullptr;
    int dpi_ = 96;
    HFONT regular_ = nullptr;
    HFONT bold_ = nullptr;
    HFONT logo_ = nullptr;
    HFONT mono_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    HBRUSH lcdBrush_ = nullptr;
    std::vector<Control> controls_;
    bool settingsOpen_ = false;
    bool checkingConnection_ = false;
    Connection checkedConnection_;
    bool panelDirty_ = true;
    bool mathSettingsSaveFailed_ = false;
    ScientificDisplay science_;
    Mode lastMode_ = Mode::calculator;
    std::wstring settingsFile_ = settingsPath();
    Settings settings_ = Settings::load(settingsFile_);
    Lcd lcd_;
    LocalProxy local_;
    std::wstring activeLocalUrl_;
    DesktopChat chat_;
    App app_{lcd_, chat_};
};

HWND Window::control(const wchar_t* type, const wchar_t* title, DWORD style, int id,
                     int x, int y, int w, int h, Panel panel, HFONT font) {
    const auto handle = CreateWindowExW(0, type, title, WS_CHILD | style, px(x), px(y),
        px(w), px(h), window_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(handle, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : regular_), FALSE);
    controls_.push_back({handle, panel});
    return handle;
}

HWND Window::label(const wchar_t* title, int x, int y, int w, int h, Panel panel, HFONT font) {
    return control(L"STATIC", title, SS_NOPREFIX, 0, x, y, w, h, panel, font);
}

void Window::button(int id, const wchar_t* title, int x, int y, int w, int h, Panel panel, Style style) {
    const auto handle = control(L"BUTTON", title, WS_TABSTOP | BS_OWNERDRAW, id, x, y, w, h, panel, bold_);
    SetWindowLongPtrW(handle, GWLP_USERDATA, style);
}

void Window::build() {
    label(L"Calcu", 28, 16, 190, 45, Panel::common, logo_);
    button(switchMode, L"Cambiar modo", 390, 24, 160, 35, Panel::common, mutedButton);
    modeButton_ = GetDlgItem(window_, switchMode);
    button(preferences, L"Configuración", 561, 24, 129, 35, Panel::common, mutedButton);
    heading_ = label(L"", 28, 80, 280, 24, Panel::common, bold_);
    status_ = label(L"", 360, 80, 330, 24, Panel::common);
    lcd_.rows[0] = label(L"", 64, 127, 584, 45, Panel::chat, mono_);
    lcd_.rows[1] = label(L"", 64, 171, 584, 45, Panel::chat, mono_);
    hint_ = label(L"", 29, 280, 662, 42, Panel::common);

    label(L"Teclado", 28, 322, 242, 27, Panel::common, bold_);
    for (int i = 0; i < 12; ++i) {
        const int id = i < 9 ? digitBase + i + 1 : i == 9 ? star : i == 10 ? digitBase : hash;
        const std::wstring caption = i < 9 ? std::to_wstring(i + 1) : i == 9 ? L"*" : i == 10 ? L"0" : L"#";
        button(id, caption.c_str(), 28 + 83 * (i % 3), 354 + 59 * (i / 3), 76, 50, Panel::common);
    }

    label(L"Funciones", 300, 322, 391, 27, Panel::calculator, bold_);
    for (std::size_t i = 0; i < symbols.size(); ++i) {
        button(symbolBase + static_cast<int>(i), symbols[i].label, 300 + 79 * (i % 5),
            354 + 38 * (i / 5), 73, 32, Panel::calculator);
    }
    button(angle, L"DEG", 537, 544, 73, 32, Panel::calculator, mutedButton);
    angleButton_ = GetDlgItem(window_, angle);
    button(425, L"Ans", 616, 544, 73, 32, Panel::calculator);
    button(historyOlder, L"‹ Historial", 28, 590, 119, 34, Panel::calculator, mutedButton);
    button(historyNewer, L"Historial ›", 154, 590, 116, 34, Panel::calculator, mutedButton);
    button(formatKey, L"Decimal", 300, 590, 111, 34, Panel::calculator, mutedButton);
    button(clear, L"Limpiar", 419, 590, 89, 34, Panel::calculator, mutedButton);
    button(erase, L"Borrar", 516, 590, 173, 34, Panel::calculator);
    button(cursorLeft, L"←", 28, 632, 119, 34, Panel::calculator);
    button(cursorRight, L"→", 154, 632, 116, 34, Panel::calculator);
    button(go, L"=  Calcular", 300, 632, 389, 34, Panel::calculator, primary);

    label(L"Escritura y lectura", 300, 322, 390, 27, Panel::chat, bold_);
    button(letters, L"ABC / 123", 300, 354, 167, 45, Panel::chat);
    lettersButton_ = GetDlgItem(window_, letters);
    button(commit, L"Confirmar letra", 477, 354, 212, 45, Panel::chat);
    button(420, L"← Cursor", 300, 410, 89, 44, Panel::chat);
    button(421, L"Cursor →", 397, 410, 89, 44, Panel::chat);
    button(422, L"Borrar", 494, 410, 89, 44, Panel::chat);
    button(423, L"Limpiar", 591, 410, 98, 44, Panel::chat);
    button(previous, L"‹  Página anterior", 300, 467, 191, 45, Panel::chat);
    button(next, L"Página siguiente  ›", 499, 467, 190, 45, Panel::chat);
    button(424, L"Enviar pregunta", 300, 537, 389, 44, Panel::chat, primary);
    sendButton_ = GetDlgItem(window_, 424);
    button(conversationHistory, L"Historial", 300, 593, 389, 39, Panel::chat, mutedButton);
    button(setupOpenAI, L"Configurar OpenAI", 28, 593, 242, 39, Panel::chat, mutedButton);

    label(L"Configuración", 28, 83, 650, 34, Panel::settings, bold_);
    label(L"Combinación para cambiar de modo", 28, 130, 380, 25, Panel::settings);
    auto prefix = control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, prefixField,
        28, 158, 125, 170, Panel::settings);
    auto last = control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, modeField,
        175, 158, 125, 180, Panel::settings);
    for (const auto symbol : {L"*", L"#"}) ComboBox_AddString(prefix, symbol);
    for (const auto symbol : {L"#", L"*", L"0", L"7", L"8", L"9"}) ComboBox_AddString(last, symbol);
    label(L"Pulsa ambas teclas en orden dentro de 1,2 segundos.", 28, 197, 665, 22, Panel::settings);
    label(L"URL HTTPS externa (vacía para usar el servidor local)", 28, 243, 665, 24, Panel::settings, bold_);
    auto url = control(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, urlField,
        28, 274, 661, 31, Panel::settings);
    SendMessageW(url, EM_SETLIMITTEXT, 1000, 0);
    label(L"Token del dispositivo", 28, 329, 665, 24, Panel::settings, bold_);
    auto token = control(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL, tokenField,
        28, 359, 661, 31, Panel::settings);
    SendMessageW(token, EM_SETLIMITTEXT, 256, 0);
    control(L"BUTTON", L"Usar respuesta de demostración local", BS_AUTOCHECKBOX | WS_TABSTOP, demoField,
        28, 413, 585, 31, Panel::settings);
    button(testConnection, L"Probar conexión", 28, 459, 181, 38, Panel::settings, mutedButton);
    button(setupOpenAISettings, L"Configurar OpenAI", 28, 508, 181, 38, Panel::settings, mutedButton);
    settingsStatus_ = label(L"", 228, 457, 460, 86, Panel::settings);
    button(cancel, L"Cancelar", 411, 550, 126, 43, Panel::settings, mutedButton);
    button(save, L"Guardar", 546, 550, 143, 43, Panel::settings, primary);
}

bool Window::create(HINSTANCE instance) {
    SetProcessDPIAware();
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    constexpr DWORD extended = WS_EX_CONTROLPARENT;
    HDC screen = GetDC(nullptr);
    dpi_ = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
    RECT workArea{};
    const bool hasWorkArea = SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0) != FALSE;
    if (hasWorkArea) {
        RECT chrome{};
        AdjustWindowRectEx(&chrome, style, FALSE, extended);
        dpi_ = std::max(48, std::min({dpi_,
            MulDiv(workArea.right - workArea.left - (chrome.right - chrome.left) - 16, 96, width),
            MulDiv(workArea.bottom - workArea.top - (chrome.bottom - chrome.top) - 16, 96, height)}));
    }
    regular_ = Fonts::make(px(16), FW_NORMAL, L"Segoe UI");
    bold_ = Fonts::make(px(16), FW_SEMIBOLD, L"Segoe UI");
    logo_ = Fonts::make(px(38), FW_BOLD, L"Segoe UI");
    mono_ = Fonts::make(px(40), FW_SEMIBOLD, L"Consolas");
    backgroundBrush_ = CreateSolidBrush(background);
    lcdBrush_ = CreateSolidBrush(lcdBackground);

    WNDCLASSW cls{};
    cls.hInstance = instance;
    cls.lpfnWndProc = dispatch;
    cls.lpszClassName = L"CalcuDesktop";
    cls.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = backgroundBrush_;
    if (!RegisterClassW(&cls)) return false;

    RECT bounds{0, 0, px(width), px(height)};
    AdjustWindowRectEx(&bounds, style, FALSE, extended);
    const int windowWidth = bounds.right - bounds.left;
    const int windowHeight = bounds.bottom - bounds.top;
    const int x = hasWorkArea ? (workArea.left + workArea.right - windowWidth) / 2 : CW_USEDEFAULT;
    const int y = hasWorkArea ? (workArea.top + workArea.bottom - windowHeight) / 2 : CW_USEDEFAULT;
    return CreateWindowExW(extended, cls.lpszClassName, L"Calcu", style, x, y, windowWidth, windowHeight,
        nullptr, nullptr, instance, this) != nullptr;
}

LRESULT CALLBACK Window::dispatch(HWND handle, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_NCCREATE) {
        auto self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        self->window_ = handle;
        SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    auto self = reinterpret_cast<Window*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    return self ? self->process(message, w, l) : DefWindowProcW(handle, message, w, l);
}

void Window::notify(const wchar_t* value) {
    textIfChanged(settingsOpen_ ? settingsStatus_ : status_, value);
}

void Window::refresh() {
    const auto current = app_.mode();
    const bool isChat = current == Mode::chat;
    if (lastMode_ != current || panelDirty_) {
        for (const auto& item : controls_) {
            const bool visible = settingsOpen_ ? item.panel == Panel::settings :
                item.panel == Panel::common || item.panel == (isChat ? Panel::chat : Panel::calculator);
            ShowWindow(item.window, visible ? SW_SHOWNA : SW_HIDE);
        }
        lastMode_ = current;
        panelDirty_ = false;
        InvalidateRect(window_, nullptr, TRUE);
    }
    if (settingsOpen_) return;

    const auto shortcut = app_.shortcut();
    const std::wstring sequence{static_cast<wchar_t>(shortcut.prefix), static_cast<wchar_t>(shortcut.mode)};
    textIfChanged(modeButton_, L"Modo  " + sequence);
    textIfChanged(heading_, isChat ? L"ChatGPT" : L"Calculadora científica");
    std::wstring mathStatus = app_.angle() == Angle::degrees ? L"DEG  ·  " : L"RAD  ·  ";
    mathStatus += formatLabel(app_.resultFormat());
    if (const auto* entry = app_.historyEntry()) {
        mathStatus = L"Historial " + std::to_wstring(app_.historyPosition()) + L" de " +
            std::to_wstring(app_.historySize()) + (entry->angle == Angle::degrees ? L"  ·  DEG" : L"  ·  RAD");
    }
    textIfChanged(status_, isChat ?
        (usesLocalProxy() && !local_.connection().configured() ? local_.status() : chat_.statusText()) :
        mathSettingsSaveFailed_ ? L"No se pudo guardar la configuración" : mathStatus);
    textIfChanged(angleButton_, app_.angle() == Angle::degrees ? L"DEG" : L"RAD");
    textIfChanged(GetDlgItem(window_, formatKey), formatLabel(app_.resultFormat()));
    textIfChanged(GetDlgItem(window_, go), app_.historyEntry() ? L"Editar cálculo" : L"=  Calcular");
    EnableWindow(GetDlgItem(window_, historyOlder), app_.historyPosition() < app_.historySize());
    EnableWindow(GetDlgItem(window_, historyNewer), app_.historyEntry() != nullptr);
    if (!isChat) {
        RECT screen{px(28), px(108), px(691), px(273)};
        InvalidateRect(window_, &screen, FALSE);
    }
    textIfChanged(lettersButton_, app_.numbers() ? L"123  →  ABC" : L"ABC  →  123");
    textIfChanged(sendButton_, app_.readingChatHistory() ? L"Salir del historial" :
        app_.showingChatReply() ? L"Volver a la pregunta" : L"Enviar pregunta");
    const bool pending = app_.chatStatus() == ChatStatus::pending;
    EnableWindow(sendButton_, !pending && (!usesLocalProxy() || local_.connection().configured() || app_.showingChatReply()));
    EnableWindow(GetDlgItem(window_, setupOpenAI), usesLocalProxy());
    const auto historyButton = GetDlgItem(window_, conversationHistory);
    textIfChanged(historyButton, L"Historial de conversación (" + std::to_wstring(app_.chatHistorySize()) + L")");
    EnableWindow(historyButton, !pending && app_.chatHistorySize() > 0);
    textIfChanged(hint_, isChat ?
        L"Pulsa varias veces para elegir una letra. Las flechas editan y los botones de página recorren el texto." :
        app_.showingPalette() ? textOf(lcd_.rows[0]) + L"   " + textOf(lcd_.rows[1]) :
        L"ⁿ√(valor, índice)  ·  logₐ(valor, base)  ·  nCr(n, r)  ·  nPr(n, r).");
    for (int i = 0; i < 10; ++i) {
        textIfChanged(GetDlgItem(window_, digitBase + i), isChat && !app_.numbers() ? abc[i] : std::to_wstring(i));
    }
}

void Window::showSettings() {
    settingsOpen_ = true;
    panelDirty_ = true;
    const auto selected = [&](int id, char c) {
        const wchar_t value[]{static_cast<wchar_t>(c), L'\0'};
        ComboBox_SelectString(GetDlgItem(window_, id), -1, value);
    };
    selected(prefixField, settings_.shortcut.prefix);
    selected(modeField, settings_.shortcut.mode);
    SetDlgItemTextW(window_, urlField, settings_.connection.url.c_str());
    SetDlgItemTextW(window_, tokenField, settings_.connection.token.c_str());
    CheckDlgButton(window_, demoField, settings_.connection.demo ? BST_CHECKED : BST_UNCHECKED);
    notify(checkingConnection_ ? L"Comprobando HTTPS, token y OpenAI…" : L"");
    refresh();
    SetFocus(GetDlgItem(window_, prefixField));
}

Connection Window::connectionFromFields() const {
    return {IsDlgButtonChecked(window_, demoField) == BST_CHECKED,
            textOf(GetDlgItem(window_, urlField)), textOf(GetDlgItem(window_, tokenField))};
}

void Window::checkConnection() {
    checkedConnection_ = connectionFromFields();
    if (checkedConnection_.url.empty() && checkedConnection_.token.empty()) {
        local_.start(GetTickCount());
        checkedConnection_ = local_.connection();
        if (!checkedConnection_.configured()) return notify(local_.status());
    }
    const auto reply = chat_.check(checkedConnection_, GetTickCount());
    if (reply.status != ChatStatus::pending) {
        showConnectionCheck(reply);
        return;
    }
    checkingConnection_ = true;
    EnableWindow(GetDlgItem(window_, testConnection), FALSE);
    notify(L"Comprobando HTTPS, token y OpenAI…");
}

void Window::updateConnectionCheck() {
    if (!checkingConnection_) return;
    const auto reply = chat_.pollCheck(GetTickCount());
    if (reply.status == ChatStatus::pending || reply.status == ChatStatus::idle) return;
    checkingConnection_ = false;
    EnableWindow(GetDlgItem(window_, testConnection), TRUE);
    if (!settingsOpen_) return;
    const auto current = connectionFromFields();
    const auto& expected = checkedConnection_.loopback && current.url.empty() && current.token.empty()
        ? local_.connection() : current;
    if (expected.url != checkedConnection_.url || expected.token != checkedConnection_.token) {
        notify(L"Los datos cambiaron. Vuelve a probar la conexión.");
        return;
    }
    showConnectionCheck(reply);
}

void Window::showConnectionCheck(const ConnectionCheck& result) {
    const auto label = [&](unsigned step) -> const wchar_t* {
        if (result.completed >= step) return L"Correcto";
        return step == result.completed + 1 ? L"Sin confirmar" : L"Pendiente";
    };
    auto message = std::wstring(checkedConnection_.loopback ? L"Servidor local: " : L"Servidor HTTPS: ") + label(1) + L"\r\nToken: " +
        label(2) + L"\r\nOpenAI: " + label(3);
    if (!result.detail.empty()) {
        message += L"\r\n";
        message.append(result.detail.begin(), result.detail.end());
    }
    notify(message.c_str());
}

bool Window::usesLocalProxy() const {
    const auto& connection = settings_.connection;
    return !connection.demo && connection.url.empty() && connection.token.empty();
}

void Window::applyConnection() {
    if (!usesLocalProxy()) {
        local_.stop();
        activeLocalUrl_.clear();
        chat_.configure(settings_.connection);
        return;
    }
    local_.start(GetTickCount());
    auto current = local_.connection();
    current.loopback = true;
    activeLocalUrl_ = current.url;
    chat_.configure(std::move(current));
}

void Window::updateLocalProxy() {
    local_.poll(GetTickCount());
    if (usesLocalProxy() && activeLocalUrl_ != local_.connection().url) {
        auto current = local_.connection();
        current.loopback = true;
        activeLocalUrl_ = current.url;
        chat_.configure(std::move(current));
    }
    if (local_.setupFailed()) {
        MessageBoxW(window_, L"El asistente necesita Python 3 con Tkinter. Revisa el README.",
                    L"Configurar OpenAI", MB_OK | MB_ICONWARNING);
    }
    if (usesLocalProxy() && !settingsOpen_ && app_.mode() == Mode::chat && chat_.takeLocalSetupNeeded()) {
        setupOpenAIKey();
    }
}

void Window::setupOpenAIKey() {
    if (usesLocalProxy()) local_.start(GetTickCount());
    if (!local_.configureKey()) {
        MessageBoxW(window_, L"Instala Python 3 con Tkinter y conserva la carpeta server junto a Calcu.exe.",
                    L"Configurar OpenAI", MB_OK | MB_ICONWARNING);
    }
}

void Window::storeSettings() {
    const auto selected = [&](int id) {
        const HWND combo = GetDlgItem(window_, id);
        wchar_t value[2]{};
        const int index = ComboBox_GetCurSel(combo);
        if (index != CB_ERR) ComboBox_GetLBText(combo, index, value);
        return static_cast<char>(value[0]);
    };
    Settings candidate = settings_;
    candidate.shortcut = {selected(prefixField), selected(modeField)};
    candidate.connection = connectionFromFields();
    if (!candidate.shortcut.valid()) return notify(L"Selecciona las dos teclas del atajo.");
    if (candidate.connection.url.empty() != candidate.connection.token.empty()) {
        return notify(L"Completa URL y token para el servidor externo, o deja los dos vacíos.");
    }
    if (!candidate.connection.url.empty() && !validUrl(candidate.connection.url)) {
        return notify(L"Introduce una URL HTTPS válida, por ejemplo https://servidor/ask.");
    }
    if (!candidate.connection.token.empty() && !validToken(candidate.connection.token)) {
        return notify(L"El token debe tener entre 32 y 256 caracteres ASCII sin espacios.");
    }
    const bool changedConnection = candidate.connection.demo != settings_.connection.demo ||
        candidate.connection.url != settings_.connection.url || candidate.connection.token != settings_.connection.token;
    if (changedConnection && app_.chatStatus() == ChatStatus::pending) {
        return notify(L"Espera a que termine la consulta antes de cambiar la conexión.");
    }
    if (!candidate.save(settingsFile_)) return notify(L"No se pudo guardar calcu.ini en esta carpeta.");
    if (changedConnection) app_.resetConversation();

    settings_ = std::move(candidate);
    mathSettingsSaveFailed_ = false;
    app_.configureShortcut(settings_.shortcut);
    if (changedConnection) applyConnection();
    app_.tick(GetTickCount());
    command(cancel);
}

void Window::drawButton(const DRAWITEMSTRUCT& item) {
    const auto style = static_cast<Style>(GetWindowLongPtrW(item.hwndItem, GWLP_USERDATA));
    const bool down = (item.itemState & ODS_SELECTED) != 0;
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const auto fill = disabled ? RGB(39, 48, 61) : style == primary
        ? (down ? RGB(145, 182, 120) : RGB(192, 221, 170))
        : down ? RGB(78, 99, 121) : style == mutedButton ? RGB(33, 45, 57) : RGB(46, 61, 78);
    const auto brush = CreateSolidBrush(fill);
    auto oldBrush = SelectObject(item.hDC, brush);
    auto oldPen = SelectObject(item.hDC, GetStockObject(NULL_PEN));
    RoundRect(item.hDC, item.rcItem.left, item.rcItem.top, item.rcItem.right, item.rcItem.bottom, px(12), px(12));
    SelectObject(item.hDC, oldPen);
    SelectObject(item.hDC, oldBrush);
    DeleteObject(brush);
    auto oldFont = SelectObject(item.hDC, bold_);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, disabled ? muted : style == primary ? RGB(25, 48, 31) : ink);
    auto rect = item.rcItem;
    const auto caption = textOf(item.hwndItem);
    DrawTextW(item.hDC, caption.c_str(), -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    SelectObject(item.hDC, oldFont);
    if (item.itemState & ODS_FOCUS) {
        InflateRect(&rect, -px(4), -px(4));
        DrawFocusRect(item.hDC, &rect);
    }
}

void Window::paint() {
    PAINTSTRUCT paint{};
    const HDC dc = BeginPaint(window_, &paint);
    if (!settingsOpen_) {
        const auto border = CreateSolidBrush(RGB(74, 102, 70));
        auto oldBrush = SelectObject(dc, border);
        auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        const bool calculation = app_.mode() == Mode::calculator;
        RoundRect(dc, px(28), px(108), px(690), px(273), px(19), px(19));
        SelectObject(dc, lcdBrush_);
        RoundRect(dc, px(34), px(114), px(684), px(267), px(14), px(14));
        if (calculation) {
            RECT separator{px(48), px(202), px(670), px(203)};
            const auto rule = CreateSolidBrush(RGB(160, 185, 136));
            FillRect(dc, &separator, rule);
            DeleteObject(rule);
            science_.paint(dc, mathInput(), mathOutput(), app_, dpi_);
        }
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(border);
    }
    EndPaint(window_, &paint);
}

void Window::action(Action key, std::string_view symbol) {
    app_.pressAction({key, '\0', symbol}, GetTickCount());
    persistMathSettings();
    refresh();
}

void Window::persistMathSettings() {
    if (settings_.angle == app_.angle() && settings_.format == app_.resultFormat()) return;
    settings_.angle = app_.angle();
    settings_.format = app_.resultFormat();
    mathSettingsSaveFailed_ = !settings_.save(settingsFile_);
}

void Window::raw(char key) {
    app_.press(key, GetTickCount());
    persistMathSettings();
    refresh();
}

void Window::command(int id) {
    if (settingsOpen_) {
        if (id == save) storeSettings();
        if (id == testConnection) checkConnection();
        if (id == setupOpenAISettings) setupOpenAIKey();
        if (id == cancel) {
            settingsOpen_ = false;
            panelDirty_ = true;
            refresh();
            SetFocus(GetDlgItem(window_, preferences));
        }
        return;
    }
    if (id >= digitBase && id < digitBase + 10) return raw(static_cast<char>('0' + id - digitBase));
    if (id >= symbolBase && id < symbolBase + static_cast<int>(symbols.size())) {
        return action(Action::symbol, symbols[id - symbolBase].token);
    }

    switch (id) {
        case star: raw('*'); break;
        case hash: raw('#'); break;
        case switchMode: action(Action::mode); break;
        case preferences: showSettings(); break;
        case erase:
        case 422: action(Action::erase); break;
        case clear:
        case 423: action(Action::clear); break;
        case cursorLeft:
        case 420: action(Action::cursorLeft); break;
        case cursorRight:
        case 421: action(Action::cursorRight); break;
        case go:
        case 424: action(Action::enter); break;
        case letters: action(Action::paletteOrNumbers); break;
        case commit: action(Action::angleOrCommit); break;
        case setupOpenAI: if (usesLocalProxy()) setupOpenAIKey(); break;
        case conversationHistory: action(Action::chatHistory); break;
        case previous: action(Action::left); break;
        case next: action(Action::right); break;
        case 425: action(Action::symbol, "Ans"); break;
        case formatKey: action(Action::resultFormat); break;
        case historyOlder: action(Action::historyOlder); break;
        case historyNewer: action(Action::historyNewer); break;
        case angle: {
            app_.setAngle(app_.angle() == Angle::degrees ? Angle::radians : Angle::degrees);
            persistMathSettings();
            app_.tick(GetTickCount());
            refresh();
            break;
        }
    }
}

bool Window::key(const MSG& msg) {
    if (msg.message == WM_KEYDOWN && settingsOpen_ && msg.wParam == VK_ESCAPE) {
        command(cancel);
        return true;
    }
    if (settingsOpen_) return false;
    if (msg.message == WM_KEYDOWN) {
        if (GetKeyState(VK_CONTROL) < 0 || GetKeyState(VK_MENU) < 0) return false;
        switch (msg.wParam) {
            case VK_RETURN: action(Action::enter); return true;
            case VK_BACK: action(Action::erase); return true;
            case VK_LEFT: action(Action::cursorLeft); return true;
            case VK_RIGHT: action(Action::cursorRight); return true;
            case VK_PRIOR: action(Action::left); return true;
            case VK_NEXT: action(Action::right); return true;
            case VK_MULTIPLY:
                if (app_.mode() == Mode::calculator) {
                    action(Action::symbol, "*");
                    return true;
                }
                break;
        }
    }
    if (msg.message == WM_CHAR && msg.wParam >= L' ' && msg.wParam <= L'~') {
        const auto c = static_cast<char>(msg.wParam);
        if ((c >= '0' && c <= '9') || c == '*' || c == '#') {
            raw(c);
            return true;
        }
        if (app_.mode() == Mode::calculator && std::string_view("+-/.%^()!,E").find(c) != std::string_view::npos) {
            action(Action::symbol, std::string_view(&c, 1));
            return true;
        }
    }
    return false;
}

LRESULT Window::process(UINT message, WPARAM w, LPARAM l) {
    switch (message) {
        case WM_CREATE:
            build();
            app_.configureShortcut(settings_.shortcut);
            app_.setAngle(settings_.angle);
            app_.setResultFormat(settings_.format);
            applyConnection();
            app_.tick(GetTickCount());
            refresh();
            SetTimer(window_, 1, 50, nullptr);
            return 0;
        case WM_TIMER:
            app_.tick(GetTickCount());
            updateLocalProxy();
            updateConnectionCheck();
            if (app_.mode() == Mode::chat) refresh();
            return 0;
        case WM_COMMAND: {
            const int id = LOWORD(w);
            const bool rawKey = (id >= digitBase && id < digitBase + 10) || id == star || id == hash;
            if (HIWORD(w) == BN_CLICKED || (rawKey && HIWORD(w) == BN_DOUBLECLICKED)) command(id);
            return 0;
        }
        case WM_DRAWITEM:
            drawButton(*reinterpret_cast<DRAWITEMSTRUCT*>(l));
            return TRUE;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            const auto dc = reinterpret_cast<HDC>(w);
            const auto control = reinterpret_cast<HWND>(l);
            const bool lcd = control == lcd_.rows[0] || control == lcd_.rows[1];
            SetTextColor(dc, lcd ? lcdInk : control == hint_ || control == status_ ? muted : ink);
            SetBkColor(dc, lcd ? lcdBackground : background);
            return reinterpret_cast<LRESULT>(lcd ? lcdBrush_ : backgroundBrush_);
        }
        case WM_LBUTTONDOWN: {
            const POINT point{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            const auto input = mathInput();
            if (!settingsOpen_ && app_.mode() == Mode::calculator && PtInRect(&input, point)) {
                app_.moveMathCursor(science_.nearest(point), GetTickCount());
                refresh();
            }
            return 0;
        }
        case WM_PAINT:
            paint();
            return 0;
        case WM_DESTROY:
            KillTimer(window_, 1);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window_, message, w, l);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    Window window;
    if (!window.create(instance)) {
        MessageBoxW(nullptr, L"No se pudo abrir la ventana de Calcu.", L"Calcu", MB_OK | MB_ICONERROR);
        return 1;
    }
    ShowWindow(window.handle(), show);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (window.key(message) || IsDialogMessageW(window.handle(), &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
