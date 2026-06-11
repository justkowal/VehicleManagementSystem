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

// Includes for POSIX Network Sockets (Linux/macOS)
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>


namespace {
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

// Standard 80mm thermal receipt width in characters
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

// Helper: Pads spaces between a left string and a right string
auto justifyLine(const std::string& left, const std::string& right) -> std::string {
    int spaces = CPL - static_cast<int>(left.length() + right.length());
    if (spaces < 1) {
        spaces = 1; // Ensure at least 1 space
    }
    return left + std::string(static_cast<size_t>(spaces), ' ') + right + "\n";
}

// Helper: Safely converts integer grosz to a 2-decimal string
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

// Helper: Performs a TCP connect and sends the payload with a 2-second timeout.
// Returns true on success, false on failure or timeout.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto tryNetworkSend(const std::string& ip_addr, int port, const std::string& payload) -> bool {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }

    // Set socket to non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (flags >= 0) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK); // NOLINT(cppcoreguidelines-pro-type-vararg)
    }

    struct sockaddr_in serv_addr {};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_addr.c_str(), &serv_addr.sin_addr) <= 0) {
        close(sock);
        return false;
    }

    int res = connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    if (res < 0) {
        if (errno == EINPROGRESS) {
            fd_set fdsw;
            FD_ZERO(&fdsw); // NOLINT(hicpp-no-assembler,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
            FD_SET(sock, &fdsw); // NOLINT(hicpp-no-assembler,cppcoreguidelines-pro-bounds-array-to-pointer-decay)

            struct timeval timeout_val {};
            timeout_val.tv_sec = 2; // 2 seconds timeout
            timeout_val.tv_usec = 0;

            res = select(sock + 1, nullptr, &fdsw, nullptr, &timeout_val);
            if (res > 0) {
                int err = 0;
                socklen_t len = sizeof(err);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err == 0) {
                    res = 0; // connect succeeded
                } else {
                    res = -1; // connect failed
                }
            } else {
                res = -1; // timeout or error
            }
        } else {
            res = -1; // other connection error
        }
    }

    if (res == 0) {
        // Restore socket to blocking mode
        if (flags >= 0) {
            fcntl(sock, F_SETFL, flags); // NOLINT(cppcoreguidelines-pro-type-vararg)
        }
        send(sock, payload.c_str(), payload.size(), 0);
        close(sock);
        return true;
    }

    close(sock);
    return false;
}
} // namespace

EscPosPrinter::EscPosPrinter(std::string device_path) : device_path_(std::move(device_path)) {}

auto EscPosPrinter::sendCommand(const std::string& payload) const -> void {

    // Check if the device_path_ is an IP:PORT network address (e.g., 127.0.0.1:9100)
    size_t colon_pos = device_path_.find(':');
    if (colon_pos != std::string::npos && colon_pos > 0 && isdigit(static_cast<unsigned char>(device_path_[colon_pos + 1])) != 0) {
        std::string ip_addr = device_path_.substr(0, colon_pos);
        int port = std::stoi(device_path_.substr(colon_pos + 1));

        if (tryNetworkSend(ip_addr, port, payload)) {
            return;
        }
        throw PrinterException("Could not connect to network printer at: " + device_path_);
    }

    // Fallback: Treat device_path_ as a regular file path (or /dev/usb/lp0)
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

    // Items and Quantities
    payload += ESC_LEFT;
    payload += "Wynajem: " + vehicle_name + "\n";

    std::ostringstream amount_stream;
    amount_stream << std::fixed << std::setprecision(2) << rental_duration_hours << " h x "
                  << formatMoney(price_per_hour) << " PLN";

    // Automatically aligns text to flush left and flush right
    payload += justifyLine(amount_stream.str(), formatMoney(total_price, "A"));
    payload += SEPARATOR;

    // Taxes
    payload += justifyLine("SP. OPODATKOWANA A:", formatMoney(net_price));
    payload += justifyLine("PODATEK PTU 23% A:", formatMoney(vat_amount));
    payload += justifyLine("SUMA PTU:", formatMoney(vat_amount));
    payload += SEPARATOR;

    // Final Total
    payload += ESC_CENTER;
    payload += ESC_BOLD_ON;
    payload += ESC_DOUBLE_HW;

    payload += "DO ZAPLATY: " + formatMoney(total_price, "PLN") + "\n";

    payload += ESC_NORMAL_TEXT;
    payload += ESC_BOLD_OFF;

    payload += RECEIPT_FOOTER;

    sendCommand(payload);
}

// Self-registration — makes "escpos" available via --printer flag.
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