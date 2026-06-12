#include "EscPosPrinter.h"
#include "Exceptions.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <regex>
#include <chrono>
#include <array>
#include <cmath>

// socket includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#endif


namespace {
#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t INVALID_SOCKET_VAL = INVALID_SOCKET;
inline auto close_socket(socket_t s) -> int {
    return closesocket(s);
}

struct WinsockInit {
    WinsockInit() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }
    ~WinsockInit() {
        WSACleanup();
    }
};
static void ensureWinsock() {
    static WinsockInit init;
}
#else
using socket_t = int;
constexpr socket_t INVALID_SOCKET_VAL = -1;
inline auto close_socket(socket_t s) -> int {
    return ::close(s);
}
#endif

const std::string ESC_INIT = std::string() + char(0x1B) + char(0x40);
const std::string ESC_CENTER = std::string() + char(0x1B) + char(0x61) + char(0x01);
const std::string ESC_LEFT = std::string() + char(0x1B) + char(0x61) + char(0x00);
const std::string ESC_BOLD_ON = std::string() + char(0x1B) + char(0x45) + char(0x01);
const std::string ESC_BOLD_OFF = std::string() + char(0x1B) + char(0x45) + char(0x00);
const std::string ESC_FEED_5 = std::string() + char(0x1B) + char(0x64) + char(0x05);
const std::string ESC_FULL_CUT = std::string() + char(0x1D) + char(0x56) + char(0x00);
const std::string ESC_DOUBLE_HW = std::string() + char(0x1D) + char(0x21) + char(0x11);
const std::string ESC_NORMAL_TEXT = std::string() + char(0x1D) + char(0x21) + char(0x00);
const std::string ESC_DISABLE_HRI = std::string() + char(0x1D) + char(0x48) + char(0x00);

// 80mm thermal receipt width
const int CPL = 48;
const std::string SEPARATOR = std::string(CPL, '-') + "\n";

const std::string RECEIPT_HEADER = ESC_INIT + ESC_CENTER + ESC_BOLD_ON +
                                   "FAKE VEHICLE RENTAL SP. Z O.O.\n" + ESC_BOLD_OFF +
                                   "ul. Kosza 5\n"
                                   "37-054 Poznan, Polska\n"
                                   "NIP: 677-123-45-67\n" +
                                   ESC_LEFT + SEPARATOR;

const std::string RECEIPT_FOOTER = "\n" + ESC_CENTER +
                                   "Dziekujemy za wybranie naszych uslug!\n"
                                   "Zapraszamy ponownie.\n" +
                                   ESC_FEED_5 + ESC_FULL_CUT;

// pads spaces between left and right
auto justifyLine(const std::string& left, const std::string& right) -> std::string {
    int spaces = CPL - static_cast<int>(left.length() + right.length());
    if (spaces < 1) {
        spaces = 1; // Ensure at least 1 space
    }
    return left + std::string(static_cast<size_t>(spaces), ' ') + right + "\n";
}

// converts grosz to decimal string
auto formatMoney(int amount_grosz, const std::string& suffix = "") -> std::string {
    int zlotys = amount_grosz / 100;
    int groszy = std::abs(amount_grosz % 100);
    std::ostringstream oss;
    oss << zlotys << "." << std::setw(2) << std::setfill('0') << groszy;
    if (!suffix.empty()) {
        oss << " " << suffix;
    }
    return oss.str();
}

// tcp connect and send with 2s timeout
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto tryNetworkSend(const std::string& ip_addr, int port, const std::string& payload) -> bool {
#ifdef _WIN32
    ensureWinsock();
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET_VAL) {
        return false;
    }

    // set non-blocking
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-vararg)
    }
#endif

    struct sockaddr_in serv_addr {};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, ip_addr.c_str(), &serv_addr.sin_addr) <= 0) {
        close_socket(sock);
        return false;
    }

    int res = connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (res < 0) {
#ifdef _WIN32
        int err_code = WSAGetLastError();
        if (err_code == WSAEWOULDBLOCK) {
#else
        if (errno == EINPROGRESS) {
#endif
            fd_set fdsw;
            FD_ZERO(&fdsw); // NOLINT(hicpp-no-assembler,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            FD_SET(sock, &fdsw); // NOLINT(hicpp-no-assembler,cppcoreguidelines-pro-bounds-array-to-pointer-decay)

            struct timeval timeout_val {};
            timeout_val.tv_sec = 2; // 2s timeout
            timeout_val.tv_usec = 0;

            res = select(static_cast<int>(sock) + 1, nullptr, &fdsw, nullptr, &timeout_val);
            if (res > 0) {
                int err = 0;
#ifdef _WIN32
                int len = sizeof(err);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len) == 0 && err == 0) {
#else
                socklen_t len = sizeof(err);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
#endif
                    res = 0; // success
                } else {
                    res = -1; // failed
                }
            } else {
                res = -1; // timeout/error
            }
        } else {
            res = -1; // error
        }
    }

    if (res == 0) {
        // set blocking
#ifdef _WIN32
        u_long mode = 0;
        ioctlsocket(sock, FIONBIO, &mode);
#else
        if (flags >= 0) {
            fcntl(sock, F_SETFL, flags); // NOLINT(cppcoreguidelines-pro-type-vararg)
        }
#endif
#ifdef _WIN32
        send(sock, payload.c_str(), static_cast<int>(payload.size()), 0);
#else
        send(sock, payload.c_str(), payload.size(), 0);
#endif
        close_socket(sock);
        return true;
    }

    close_socket(sock);
    return false;
}
} // namespace

EscPosPrinter::EscPosPrinter(std::string device_path) : device_path_(std::move(device_path)) {}

auto EscPosPrinter::sendCommand(const std::string& payload) const -> void {

    // check if network address
    size_t colon_pos = device_path_.find(':');
    if (colon_pos != std::string::npos && colon_pos > 0 && isdigit(static_cast<unsigned char>(device_path_[colon_pos + 1])) != 0) {
        std::string ip_addr = device_path_.substr(0, colon_pos);
        int port = std::stoi(device_path_.substr(colon_pos + 1));

        if (tryNetworkSend(ip_addr, port, payload)) {
            return;
        }
        throw PrinterException("Could not connect to network printer at: " + device_path_);
    }

    // fallback to file path
    std::ofstream port(device_path_, std::ios::out | std::ios::binary | std::ios::app);
    if (!port.is_open()) {
        throw PrinterException("Printer offline: " + device_path_);
    }
    port << payload;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto EscPosPrinter::printCheckout(const std::string& vehicle_name,
                                  const std::string& code,
                                  const std::string& name,
                                  const std::string& surname,
                                  const std::string& id_card,
                                  std::chrono::system_clock::time_point timestamp) -> void {
    std::string payload = RECEIPT_HEADER;

    payload += ESC_CENTER;
    payload += ESC_BOLD_ON;
    payload += "POTWIERDZENIE WYPOZYCZENIA\n\n";
    payload += ESC_BOLD_OFF;

    payload += ESC_LEFT;
    payload += "Pojazd: " + vehicle_name + "\n";
    if (!name.empty() || !surname.empty()) {
        payload += "Najemca: " + name + " " + surname + "\n";
    }
    if (!id_card.empty()) {
        payload += "ID: " + id_card + "\n";
    }
    if (timestamp != std::chrono::system_clock::time_point{}) {
        std::time_t raw_time = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_struct{};
#ifdef _WIN32
        localtime_s(&tm_struct, &raw_time);
#else
        localtime_r(&raw_time, &tm_struct);
#endif
        std::array<char, 20> buffer{};
        std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M:%S", &tm_struct);
        payload += "Data: " + std::string(buffer.data()) + "\n";
    }
    payload += "\n";

    payload += ESC_CENTER;
    payload += "KOD SZYBKIEGO ZWROTU:\n";
    payload += ESC_DOUBLE_HW;
    payload += code + "\n";
    payload += ESC_NORMAL_TEXT;

    payload += ESC_DISABLE_HRI;
    payload += std::string() + char(0x1D) + char(0x6B) + char(69) + char(code.length()) + code;

    payload += RECEIPT_FOOTER;

    sendCommand(payload);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto EscPosPrinter::printReturn(const std::string& vehicle_name, int price_per_hour, double rental_duration_hours, int total_price) -> void {
    int net_price = static_cast<int>(std::round(static_cast<double>(total_price) / 1.23));
    int vat_amount = total_price - net_price;

    std::string payload = RECEIPT_HEADER;

    payload += ESC_CENTER;
    payload += ESC_BOLD_ON;
    payload += "PARAGON FISKALNY\n\n";
    payload += ESC_BOLD_OFF;

    payload += ESC_LEFT;
    payload += "Wynajem: " + vehicle_name + "\n";

    std::ostringstream amount_stream;
    amount_stream << std::fixed << std::setprecision(2) << rental_duration_hours << " h x "
                  << formatMoney(price_per_hour) << " PLN";

    payload += justifyLine(amount_stream.str(), formatMoney(total_price, "A"));
    payload += SEPARATOR;

    payload += justifyLine("SP. OPODATKOWANA A:", formatMoney(net_price));
    payload += justifyLine("PODATEK PTU 23% A:", formatMoney(vat_amount));
    payload += justifyLine("SUMA PTU:", formatMoney(vat_amount));
    payload += SEPARATOR;

    payload += ESC_CENTER;
    payload += ESC_BOLD_ON;
    payload += ESC_DOUBLE_HW;

    payload += "DO ZAPLATY: " + formatMoney(total_price, "PLN") + "\n";

    payload += ESC_NORMAL_TEXT;
    payload += ESC_BOLD_OFF;

    payload += RECEIPT_FOOTER;

    sendCommand(payload);
}

// register escpos
#include "PrinterRegistry.h"
REGISTER_PRINTER_WITH_VALIDATOR("escpos", EscPosPrinter, [](const std::string& address) -> std::optional<std::string> {
    if (address.empty()) {
        return "Printer address/port cannot be empty.";
    }
    static const std::regex address_regex(R"(^([a-zA-Z0-9.-]+):([0-9]+)$)");
    if (!std::regex_match(address, address_regex)) {
        return "Printer address must be in 'host:port' format (e.g., 127.0.0.1:9100).";
    }
    return std::nullopt;
}, true);