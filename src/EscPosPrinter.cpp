#include "EscPosPrinter.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace {
    // esc/pos bytes
    const std::string ESC_INIT = std::string() + char(0x1B) + char(0x40);
    const std::string ESC_CENTER = std::string() + char(0x1B) + char(0x61) + char(0x01);
    const std::string ESC_LEFT = std::string() + char(0x1B) + char(0x61) + char(0x00);
    const std::string ESC_BOLD_ON = std::string() + char(0x1B) + char(0x45) + char(0x01);
    const std::string ESC_BOLD_OFF = std::string() + char(0x1B) + char(0x45) + char(0x00);
    const std::string ESC_FEED_5 = std::string() + char(0x1B) + char(0x64) + char(0x05);
    const std::string ESC_FULL_CUT = std::string() + char(0x1D) + char(0x56) + char(0x00);
    const std::string ESC_DOUBLE_HW = std::string() + char(0x1D) + char(0x21) + char(0x11);
    const std::string ESC_NORMAL_TEXT = std::string() + char(0x1D) + char(0x21) + char(0x00);

    const std::string RECEIPT_HEADER =
        ESC_INIT + ESC_CENTER + ESC_BOLD_ON +
        "FAKE VEHICLE RENTAL SP. Z O.O.\n" +
        ESC_BOLD_OFF +
        "ul. Kosza 5\n"
        "37-054 Poznan, Polska\n"
        "NIP: 677-123-45-67\n"
        "--------------------------------\n";

    const std::string RECEIPT_FOOTER =
        "\n" + ESC_CENTER +
        "Dziekujemy za wybranie naszych uslug!\n"
        "Zapraszamy ponownie.\n" +
        ESC_FEED_5 +
        ESC_FULL_CUT;
}

EscPosPrinter::EscPosPrinter(std::string device_path) 
    : device_path_(std::move(device_path)) {}

auto EscPosPrinter::sendCommand(const std::string& payload) const -> void {
    std::ofstream port(device_path_, std::ios::out | std::ios::binary | std::ios::app);
    if (port.is_open()) {
        port << payload;
    } else {
        std::cerr << "[Warning] Printer offline: " << device_path_ << "\n";
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto EscPosPrinter::printCheckout(const std::string& vehicle_name, const std::string& code) -> void {
    std::string payload = RECEIPT_HEADER;
    
    payload += ESC_CENTER;   // center
    payload += ESC_BOLD_ON;   // bold on
    payload += "POTWIERDZENIE WYPOZYCZENIA\n\n";
    payload += std::string() + char(0x1B) + char(0x45) + char(0x00);   // bold off

    payload += ESC_LEFT;   // left
    payload += "Pojazd: " + vehicle_name + "\n\n";
    
    payload += ESC_CENTER;   // center
    payload += "KOD SZYBKIEGO ZWROTU:\n";
    payload += ESC_DOUBLE_HW;   // double hw
    payload += code + "\n";
    payload += ESC_NORMAL_TEXT;   // normal
    
    payload += RECEIPT_FOOTER;
    
    sendCommand(payload);
}

auto EscPosPrinter::printReturn(const std::string& vehicle_name, double price_per_hour, double rental_duration_hours, double total_price) -> void {
    double net_price = total_price / 1.23;
    double vat_amount = total_price - net_price;
    
    std::ostringstream receipt;
    receipt << std::fixed << std::setprecision(2);
    
    std::string payload = RECEIPT_HEADER;
    
    payload += ESC_CENTER;   // center
    payload += ESC_BOLD_ON;   // bold on
    payload += "PARAGON FISKALNY\n\n";
    payload += ESC_BOLD_OFF;   // bold off

    payload += ESC_LEFT;   // left
    receipt << "Wynajem: " << vehicle_name << "\n";
    receipt << rental_duration_hours << " h x " << price_per_hour << " PLN\n";
    receipt << "                            " << total_price << " A\n"; // tax bracket a
    receipt << "--------------------------------\n";

    receipt << "SP. OPODATKOWANA A:         " << net_price << "\n";
    receipt << "PODATEK PTU 23% A:          " << vat_amount << "\n";
    receipt << "SUMA PTU:                   " << vat_amount << "\n";
    receipt << "--------------------------------\n";
    
    payload += receipt.str();
    
    payload += ESC_CENTER;   // center
    payload += ESC_BOLD_ON;   // bold on
    payload += ESC_DOUBLE_HW;   // double hw
    
    std::ostringstream final_total;
    final_total << std::fixed << std::setprecision(2) << "DO ZAPLATY: " << total_price << " PLN\n";
    payload += final_total.str();
    
    payload += ESC_NORMAL_TEXT;   // normal
    payload += ESC_BOLD_OFF;   // bold off

    payload += RECEIPT_FOOTER;
    
    sendCommand(payload);
}