#include "app.hpp"
#include "config.hpp"
#include "hardware_profile.hpp"
#include "wifi_retry.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
using namespace calcu;

void check(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FALLO: " << message << '\n';
    std::exit(1);
}

void value(std::string_view expression, double expected, Angle angle = Angle::degrees) {
    const auto result = evaluate(expression, angle);
    check(result.error == MathError::none && std::abs(result.value - expected) < 1e-10,
          std::string(expression).c_str());
}

void error(std::string_view expression, MathError expected) {
    check(evaluate(expression, Angle::degrees).error == expected, std::string(expression).c_str());
}

void mathematics() {
    value("2+3*4-8/2", 10);
    value("(2+3)*4", 20);
    value("2^3^2", 512);
    value("-2^2", -4);
    value("(-2)^2", 4);
    value("2^-2", 0.25);
    value("200*10%", 20);
    value("sqrt(81)+cbrt(-8)", 7);
    value("sin(30)+cos(60)+tan(45)", 2);
    value("sin(pi/2)", 1, Angle::radians);
    value("log(100)+ln(e)", 3);
    value(" .5 + 2.5e-1 ", 0.75);
    error("1/0", MathError::divideByZero);
    error("0^-1", MathError::divideByZero);
    error("sqrt(-1)", MathError::domain);
    error("log(0)", MathError::domain);
    error("tan(90)", MathError::domain);
    error("1e309", MathError::range);
    error("10^1000", MathError::range);
    error("1e", MathError::syntax);
    error("(3+4", MathError::syntax);
    error("2(3)", MathError::syntax);
    error("", MathError::syntax);
    error(std::string(26, '-') + "2", MathError::complexity);
    error(std::string(config::expressionLimit + 1, '1'), MathError::complexity);
}

void fraction(std::string_view expression, int64_t numerator, int64_t denominator) {
    const auto result = evaluate(expression, Angle::degrees);
    check(result.error == MathError::none && result.exact &&
        result.exact->numerator == numerator && result.exact->denominator == denominator,
        std::string(expression).c_str());
}

void scientificMath() {
    fraction("1/3+1/6", 1, 2);
    fraction("0.1+0.2", 3, 10);
    fraction("0.0000000000000000002", 1, 5000000000000000000LL);
    fraction("922337203685477580.8", 4611686018427387904LL, 5);
    fraction("(1/2)^-3", 8, 1);
    fraction("1/(-2)", -1, 2);
    fraction("(-1)^9007199254740993", -1, 1);
    fraction("9223372036854775807/2*2/9223372036854775807", 1, 1);
    fraction("root(-27/8,3)", -3, 2);
    fraction("root(-8,-3)", -1, 2);
    fraction("sqrt(81/100)", 9, 10);
    fraction("cbrt(-125)", -5, 1);
    fraction("2.5E-3", 1, 400);
    fraction("20!", 2432902008176640000LL, 1);
    fraction("0! + 2^3! + (-3)^2", 74, 1);
    fraction("ncr(66,33)", 7219428434016265740LL, 1);
    fraction("ncr(5,0)+npr(8,0)", 2, 1);
    fraction("npr(8,3)", 336, 1);
    fraction("log(1000)+log(1/8,2)", 0, 1);
    fraction("logb(9007199254740993/9007199254740992,9007199254740993/9007199254740992)", 1, 1);
    fraction("log(100)!", 2, 1);
    value("asin(.5)+acos(.5)+atan(1)", 135);
    value("asin(1)", 1.5707963267948966, Angle::radians);
    value("root(16,2.5)", 3.0314331330207964);
    check(evaluate("tan(89.99999999999997)", Angle::degrees).error == MathError::none,
          "Tangente finita cerca del polo");
    check(evaluate("1e-320", Angle::degrees).value > 0, "Literal subnormal");
    check(!evaluate("sqrt(2)", Angle::degrees).exact, "Raiz irracional conserva aproximacion");
    check(!evaluate("21!", Angle::degrees).exact, "Desborde de entero conserva aproximacion");
    error("(-1)!", MathError::domain);
    error("2.0000000000000000000001!", MathError::domain);
    error("171!", MathError::range);
    error("ncr(5,6)", MathError::domain);
    error("npr(1000,1000)", MathError::range);
    error("log(10,1)", MathError::domain);
    error("log(10,-2)", MathError::domain);
    error("root(-16,2)", MathError::domain);
    error("root(16,0)", MathError::domain);
    error("root(0,-1)", MathError::divideByZero);
    error("asin(2)", MathError::domain);
    error("0^0", MathError::domain);
    error("1e-200*1e-200", MathError::range);
    error("Ans", MathError::noAnswer);
    const auto third = evaluate("1/3", Angle::degrees);
    check(formatResult(third, ResultFormat::fraction) == "1/3", "Formato fraccionario");
    check(formatResult(third, ResultFormat::decimal) == "0.333333333333", "Formato decimal");
    check(formatResult(third, ResultFormat::scientific).find("e-01") != std::string::npos, "Formato cientifico");
    check(formatResult(evaluate("Ans*3", Angle::degrees, &third), ResultFormat::fraction) == "1",
          "Ans conserva la fraccion exacta");
    const auto tree = parseExpression("1/2^");
    check(tree.consumed && tree.error == MathError::syntax &&
        tree.nodes[tree.root].kind == NodeKind::binary &&
        tree.nodes[tree.nodes[tree.root].right].kind == NodeKind::binary &&
        tree.nodes.back().marker == 1, "Arbol parcial y posiciones de operadores");
}

void multitap() {
    MultiTap tap;
    tap.press('2', 10);
    tap.press('2', 100);
    tap.press('2', 200);
    check(tap.text() == "C", "ABC gira con la misma tecla");
    tap.press('2', 950);
    tap.press('0', 960);
    tap.press('0', 970);
    tap.press('1', 980);
    tap.press('1', 990);
    check(tap.text() == "CA  ,", "Vencimiento, espacios consecutivos y puntuacion");
    tap.erase();
    tap.toggleNumbers();
    tap.press('2', 1000);
    tap.press('2', 1010);
    check(tap.text() == "CA  22", "Borrado y numeros consecutivos");
    tap.clear();
    tap.toggleNumbers();
    const auto last = std::numeric_limits<uint32_t>::max();
    tap.press('9', last - 20);
    tap.press('9', 10);
    check(tap.text() == "X", "Multitap conserva el plazo al desbordar millis");
    tap.commit();
    tap.press('9', 20);
    check(tap.text() == "XW", "Confirmar letra habilita otra de la misma tecla");
    tap.clear();
    for (std::size_t i = 0; i < config::questionLimit + 1; ++i) tap.press('0', 21);
    check(tap.text().size() == config::questionLimit, "Pregunta limitada");
    tap.clear();
    tap.press('2', 50);
    tap.press('3', 60);
    tap.move(-1);
    tap.press('4', 70);
    tap.press('4', 80);
    check(tap.text() == "AHD" && tap.cursor() == 2, "Multitap corrige en medio del borrador");
    tap.erase();
    check(tap.text() == "AD" && tap.cursor() == 1, "Borrar conserva el resto del texto");
}

class RecordingDisplay : public Display {
public:
    Frame frame;
    int writes = 0;

    void write(const Frame& next, const Frame&) override {
        frame = next;
        ++writes;
    }
};

class FakeChat : public Chat {
public:
    std::string sent;
    ChatHistory sentHistory;
    ChatStatus next = ChatStatus::answer;

    ChatReply start(std::string_view question, uint32_t) override {
        sent.assign(question);
        return {ChatStatus::pending, {}};
    }

    ChatReply startWithHistory(std::string_view question, const ChatHistory& history, uint32_t now) override {
        sentHistory = history;
        return start(question, now);
    }

    ChatReply poll(uint32_t now) override {
        if (now < 2000) return {ChatStatus::pending, {}};
        return {next, next == ChatStatus::answer
            ? std::string(32, 'A') + "Pagina dos" : "Tiempo de espera"};
    }

    const char* connectionLabel() const override { return "Prueba local"; }
};

void scientificSession() {
    RecordingDisplay display;
    FakeChat chat;
    App app(display, chat);
    uint32_t time = 1;
    auto command = [&](Action action, std::string_view symbol = {}) {
        app.pressAction({action, '\0', symbol}, ++time);
    };
    app.press('1', ++time);
    command(Action::symbol, "/");
    app.press('2', ++time);
    command(Action::enter);
    check(app.visibleResult() && display.frame[0].find("0.5") == 0, "LCD formato decimal inicial");
    command(Action::resultFormat);
    check(display.frame[0].find("1/2") == 0, "LCD fraccion lineal");
    command(Action::resultFormat);
    check(display.frame[0].find("5.0000000000e-01") == 0, "LCD notacion cientifica");
    command(Action::clear);
    command(Action::symbol, "Ans");
    command(Action::symbol, "*");
    app.press('2', ++time);
    command(Action::enter);
    check(app.visibleResult()->exact->numerator == 1, "Ans preservado al limpiar");
    command(Action::historyOlder);
    command(Action::historyOlder);
    check(app.historyEntry()->expression == "1/2" && app.historyPosition() == 2, "Historial en orden");
    check(display.frame[0].find("Hist 2/2") == 0, "LCD indica posicion del historial");
    command(Action::enter);
    check(!app.historyEntry() && app.expression() == "1/2" && !app.visibleResult(), "Recupera en editor");
    app.moveMathCursor(1, ++time);
    app.press('1', ++time);
    check(app.expression() == "11/2" && app.cursor() == 2, "Insercion en posicion grafica");
    command(Action::clear);
    for (char digit : std::string("123456789")) {
        command(Action::clear);
        app.press(digit, ++time);
        command(Action::enter);
    }
    check(app.historySize() == 8, "Historial limitado a ocho resultados");
    for (int i = 0; i < 8; ++i) command(Action::historyOlder);
    check(app.historyEntry()->expression == "2", "Historial descarta el mas antiguo");
    command(Action::mode);
    command(Action::mode);
    check(app.historyPosition() == 8, "Cambio de modo conserva historial");
    command(Action::erase);
    command(Action::clear);
    for (int i = 0; i < 32; ++i) app.press('9', ++time);
    check(app.expression().size() == 32 && display.frame[1].back() == '|', "LCD sigue el cursor largo");
    app.moveMathCursor(1, ++time);
    check(display.frame[1][1] == '|', "LCD permite volver al inicio");
}

void wifiRecovery() {
    WifiRetry retry;
    check(retry.tick(100, false) == WifiAttempt::first, "WiFi inicia al encender");
    check(retry.tick(15099, false) == WifiAttempt::none, "WiFi espera entre intentos");
    check(retry.tick(15100, false) == WifiAttempt::reconnect, "WiFi reintenta sin bloquear");
    check(retry.tick(15300, true) == WifiAttempt::none, "WiFi registra conexion");
    check(retry.tick(15500, false) == WifiAttempt::reconnect, "WiFi reintenta al perder la red");
    check(retry.tick(15501, false) == WifiAttempt::none, "WiFi evita reintentos continuos");
    check(retry.tick(17000, true) == WifiAttempt::none, "WiFi acepta conexion recuperada");
    WifiRetry wrapped;
    check(wrapped.tick(UINT32_MAX - 999, false) == WifiAttempt::first, "WiFi usa reloj de 32 bits");
    check(wrapped.tick(14000, false) == WifiAttempt::reconnect, "WiFi funciona tras desbordar millis");
}

void hardwareProfiles() {
    auto output = +[](int pin) { return pin >= 0 && pin < 28; };
    auto input = +[](int pin) { return pin >= 0 && pin < 32; };
    config::Hardware profile;
    auto result = validateHardware(profile, output, input);
    check(result.keypad && result.lcd, "El perfil inicial permite serie sin GPIO");
    profile.matrixEnabled = true;
    profile.rows = {0, 1, 2, 3};
    profile.columns = {4, 5, 6};
    profile.lcdController = config::LcdController::hd44780Parallel;
    profile.lcdRs = 10;
    profile.lcdEnable = 11;
    profile.lcdData = {12, 13, 14, 15};
    result = validateHardware(profile, output, input);
    check(result.keypad && result.lcd, "Validacion con pines sinteticos independientes");
    profile.lcdData[0] = 1;
    result = validateHardware(profile, output, input);
    check(!result.keypad && !result.lcd, "Rechaza colision de buses");
    profile.lcdData[0] = 12;
    profile.columns[0] = 30;
    check(!validateHardware(profile, output, input).keypad, "Rechaza columna sin pull-up interno");
    profile.matrixInternalPullups = false;
    check(validateHardware(profile, output, input).keypad, "Admite pull-up externo");
    profile.keys[0] = profile.keys[1];
    profile.lcdRowAddresses[1] = 0x08;
    result = validateHardware(profile, output, input);
    check(!result.keypad && !result.lcd, "Rechaza mapa repetido y filas LCD solapadas");
}

void conversationContext() {
    RecordingDisplay display;
    FakeChat chat;
    App app(display, chat);
    uint32_t now = 2010;
    auto press = [&](std::string_view keys) {
        for (char key : keys) app.press(key, now += 30);
    };
    press("*#23*7*42#");
    check(chat.sent == "AD" && chat.sentHistory.empty(), "Corrige en medio con teclado fisico");
    check(app.chatHistorySize() == 1, "Guarda primer turno completo");
    press("#4#");
    check(chat.sent == "G" && chat.sentHistory.size() == 1 &&
          chat.sentHistory[0].question == "AD", "Seguimiento incluye pregunta y respuesta");
    press("*9");
    check(display.frame[0].find("Hist 1/2") == 0 &&
          display.frame[1].find("TU: G") == 0, "LCD muestra el turno reciente");
    press("*3");
    check(display.frame[0].find("GPT: ") == 0, "LCD recorre una respuesta del historial");
    press("*9");
    check(display.frame[1].find("TU: AD") == 0, "Segundo atajo consulta turno anterior");
    press("##");
    chat.next = ChatStatus::error;
    press("5#");
    check(app.chatHistorySize() == 2, "Fallo de red no se incorpora al contexto");
    press("#");
    check(display.frame[1].find("J|") == 0, "El error conserva el borrador");
    chat.next = ChatStatus::answer;
    press("#");
    for (int i = 0; i < 4; ++i) press("#2#");
    check(app.chatHistorySize() == config::chatHistoryLimit &&
          chat.sentHistory.front().question != "AD", "Limita y rota el contexto");
    app.resetConversation();
    check(app.chatHistorySize() == 0 && !app.readingChatHistory(), "Cambio de conexion limpia contexto");
    check(app.configureShortcut({'#', '7'}), "Atajo configurable");
    press("#7#7");
    check(app.mode() == Mode::chat, "Modo funciona con sufijo reservado por el usuario");
    press("23#8#42*");
    check(chat.sent == "AD" && chat.sentHistory.empty(), "Cursor y contexto siguen disponibles con otro atajo");
    check(chatRequestJson("A\"B", {{ "X\\Y", "OK" }}) ==
          R"({"question":"A\"B","history":[{"question":"X\\Y","answer":"OK"}]})",
          "Serializacion JSON compartida");
}

void modesAndDisplay() {
    RecordingDisplay display;
    FakeChat chat;
    App app(display, chat);
    uint32_t now = 0;
    auto press = [&](std::string_view sequence) {
        for (char key : sequence) app.press(key, now += 10);
    };
    app.tick(now);
    app.tick(now);
    check(app.mode() == Mode::calculator && display.writes == 1, "Arranque y LCD sin redibujos");
    press("2*1#3#");
    check(display.frame[0][0] == '5', "Evaluacion con simbolo de la paleta");
    press("*4*2*4");
    check(display.frame[1].find("2|3") == 0, "Edicion en medio de la expresion");
    press("*6");
    check(display.frame[0].find("Calcu RAD") == 0, "Grados a radianes");

    press("*");
    now += config::shortcutMs;
    app.tick(now);
    press("#");
    check(app.mode() == Mode::calculator, "El atajo caduca");
    press("*#");
    check(app.mode() == Mode::chat, "Cambio a ChatGPT");
    press("222#");
    check(chat.sent == "C" && display.frame[1].find("Consultando") == 0, "Envio con letra pendiente");
    press("*#");
    check(app.mode() == Mode::calculator, "Regreso durante una consulta");
    now = 2100;
    app.tick(now);
    press("*#");
    check(display.frame[0] == std::string(16, 'A'), "La respuesta llega en segundo plano");
    press("*3");
    check(display.frame[0].find("Pagina dos") == 0, "Avance por respuesta");
    press("*2");
    check(display.frame[0] == std::string(16, 'A'), "Retroceso por respuesta");
    press("#");
    check(display.frame[1].find("Prueba local") == 0, "Volver al editor");
    chat.next = ChatStatus::error;
    press("2#");
    check(display.frame[0].find("Error ChatGPT") == 0, "Muestra error de consulta");
    press("#");
    check(display.frame[1].find("A|") == 0, "Conserva pregunta tras un error");
    press("*5*1");
    press(std::string(32, '2') + "3*2");
    check(display.frame[0] == std::string(16, '2'), "Revisa el principio de una pregunta larga");
    press("*3");
    check(display.frame[0][0] == '3', "Revisa el final de una pregunta larga");
    press("#");
    check(chat.sent == std::string(32, '2') + "3", "Enviar desde la vista de pregunta");
    press("*#");
    check(app.mode() == Mode::calculator, "Cambio bidireccional");
    press("*4");
    check(display.frame[1].find("2|3") == 0, "Conserva expresion al cambiar de modo");
    check(!app.configureShortcut({'*', '4'}), "Protege atajo de borrado");
    check(app.configureShortcut({'#', '8'}), "Configura un atajo alternativo");
    press("#8");
    check(app.mode() == Mode::chat, "El atajo alternativo funciona");
    press("#8");
    check(app.mode() == Mode::calculator, "El atajo alternativo regresa a calculadora");
    app.pressAction({Action::clear}, now);
    app.pressAction({Action::symbol, '\0', "sqrt("}, now);
    press("9");
    app.pressAction({Action::symbol, '\0', ")"}, now);
    press("*");
    check(display.frame[0][0] == '3', "Los botones de simbolo usan el motor compartido");
}
}

int main() {
    mathematics();
    scientificMath();
    scientificSession();
    hardwareProfiles();
    wifiRecovery();
    conversationContext();
    multitap();
    modesAndDisplay();
    std::cout << "OK: motor matematico, multitap, modos y LCD\n";
}
