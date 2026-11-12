#include "local_proxy.hpp"
#include "settings.hpp"

#include <bcrypt.h>
#include <array>
#include <charconv>
#include <filesystem>

namespace calcu::desktop {
namespace {
void close(HANDLE& handle) {
    if (handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    handle = nullptr;
}

struct Handle {
    HANDLE value = nullptr;
    ~Handle() { close(value); }
    HANDLE release() { const auto result = value; value = nullptr; return result; }
};

HANDLE launch(const wchar_t* argument, HANDLE input = nullptr, HANDLE output = nullptr) {
    const auto folder = std::filesystem::path(settingsPath()).parent_path();
    const auto script = folder / L"server" / L"desktop_proxy.py";
    std::error_code issue;
    if (!std::filesystem::is_regular_file(script, issue)) return nullptr;

    wchar_t executable[MAX_PATH]{};
    auto size = SearchPathW(nullptr, L"py.exe", nullptr, MAX_PATH, executable, nullptr);
    const bool launcher = size != 0 && size < MAX_PATH;
    if (!launcher) size = SearchPathW(nullptr, L"python.exe", nullptr, MAX_PATH, executable, nullptr);
    if (!size || size >= MAX_PATH) return nullptr;
    std::wstring command = L"\"" + std::wstring(executable) + L"\"" +
        (launcher ? L" -3 -B \"" : L" -B \"") + script.wstring() + L"\" " + argument;

    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle silent;
    if (input) {
        silent.value = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &inherit, OPEN_EXISTING, 0, nullptr);
        if (silent.value == INVALID_HANDLE_VALUE) return nullptr;
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = input;
        startup.hStdOutput = output;
        startup.hStdError = silent.value;
    }
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable, command.data(), nullptr, nullptr, input != nullptr,
                        CREATE_NO_WINDOW, nullptr, folder.c_str(), &startup, &process)) return nullptr;
    CloseHandle(process.hThread);
    return process.hProcess;
}

std::string token() {
    std::array<unsigned char, 32> bytes{};
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) return {};
    constexpr char hex[] = "0123456789abcdef";
    std::string value;
    value.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        value += hex[byte >> 4];
        value += hex[byte & 15];
    }
    return value;
}
}

LocalProxy::~LocalProxy() {
    stop();
    close(setup_);
}

void LocalProxy::start(uint32_t now) {
    if (process_) return;
    status_ = L"No se pudo iniciar el servidor local";
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle childInput, parentInput, childOutput, parentOutput;
    if (!CreatePipe(&childInput.value, &parentInput.value, &inherit, 0) ||
        !CreatePipe(&parentOutput.value, &childOutput.value, &inherit, 0) ||
        !SetHandleInformation(parentInput.value, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(parentOutput.value, HANDLE_FLAG_INHERIT, 0)) return;
    auto secret = token();
    if (secret.empty()) return;
    process_ = launch(L"--serve", childInput.value, childOutput.value);
    if (!process_) {
        status_ = L"Servidor local: revisa Python 3 y server";
        return;
    }
    close(childInput.value);
    close(childOutput.value);
    token_.assign(secret.begin(), secret.end());
    secret += '\n';
    DWORD sent = 0;
    if (!WriteFile(parentInput.value, secret.data(), static_cast<DWORD>(secret.size()), &sent, nullptr) ||
        sent != secret.size()) {
        stop();
        return;
    }
    input_ = parentInput.release();
    output_ = parentOutput.release();
    started_ = now;
    status_ = L"Iniciando servidor local…";
}

void LocalProxy::stop() {
    // El servidor observa EOF. No espera a la red desde la interfaz.
    close(input_);
    close(output_);
    close(process_);
    bootstrap_.clear();
    token_.clear();
    connection_ = {};
}

void LocalProxy::poll(uint32_t now) {
    if (!process_) return;
    if (WaitForSingleObject(process_, 0) != WAIT_TIMEOUT) {
        stop();
        status_ = L"Servidor local detenido. Vuelve a iniciar";
        return;
    }
    if (!output_) return;
    DWORD available = 0;
    char buffer[32]{};
    if (!PeekNamedPipe(output_, nullptr, 0, nullptr, &available, nullptr) ||
        available > sizeof buffer || now - started_ >= 10000 ||
        (available && !ReadFile(output_, buffer, available, &available, nullptr))) {
        stop();
        status_ = L"Servidor local: revisa la instalación de Python";
        return;
    }
    bootstrap_.append(buffer, available);
    const auto newline = bootstrap_.find('\n');
    if (newline == std::string::npos && bootstrap_.size() < sizeof buffer) return;
    auto line = std::string_view(bootstrap_).substr(0, newline);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    unsigned port = 0;
    const auto parsed = std::from_chars(line.data(), line.data() + line.size(), port);
    if (newline == std::string::npos || parsed.ec != std::errc{} ||
        parsed.ptr != line.data() + line.size() || !port || port > 65535) {
        stop();
        status_ = L"Respuesta inválida del servidor local";
        return;
    }
    connection_ = {false, L"http://127.0.0.1:" + std::to_wstring(port) + L"/ask", std::move(token_), true};
    close(output_);
    status_ = L"Servidor local listo";
}

bool LocalProxy::configureKey() {
    if (setup_) return true;
    setup_ = launch(L"--configure");
    return setup_ != nullptr;
}

bool LocalProxy::setupFailed() {
    if (!setup_ || WaitForSingleObject(setup_, 0) == WAIT_TIMEOUT) return false;
    DWORD result = 0;
    const bool failed = !GetExitCodeProcess(setup_, &result) || result != 0;
    close(setup_);
    return failed;
}
}
