#include "EscPosPrinter.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace {
    const std::string RECEIPT_HEADER = 
        "\x1B\x40"       // INIT Printer
        "\x1B\x61\x01"   // Center align
        "\x1B\x45\x01"   // Bold ON
        "FAKE VEHICLE RENTAL SP. Z O.O.\n"
        "\x1B\x45\x00"   // Bold OFF
        "ul. Kosza 5\n"
        "37-054 Poznan, Polska\n"
        "NIP: 677-123-45-67\n"
        "--------------------------------\n";

    const std::string RECEIPT_FOOTER = 
        "\n\x1B\x61\x01" // Center align
        "Dziekujemy za wybranie naszych uslug!\n"
        "Zapraszamy ponownie.\n"
        "\x1B\x64\x05"   // Feed 5 lines
        "\x1D\x56\x00";  // Full Cut
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

auto EscPosPrinter::printCheckout(const std::string& vehicle_name, const std::string& code) -> void {
    std::string payload = RECEIPT_HEADER;
    
    payload += "\x1B\x61\x01";   // Center align
    payload += "\x1B\x45\x01";   // Bold ON
    payload += "POTWIERDZENIE WYPOZYCZENIA\n\n";
    payload += "\x1B\x45\x00";   // Bold OFF
    
    payload += "\x1B\x61\x00";   // Left align
    payload += "Pojazd: " + vehicle_name + "\n\n";
    
    payload += "\x1B\x61\x01";   // Center align
    payload += "KOD SZYBKIEGO ZWROTU:\n";
    payload += "\x1D\x21\x11";   // Double Height/Width
    payload += code + "\n";
    payload += "\x1D\x21\x00";   // Normal text
    
    payload += RECEIPT_FOOTER;
    
    sendCommand(payload);
}

auto EscPosPrinter::printReturn(const std::string& vehicle_name, double price_per_hour, double rental_duration_hours, double total_price) -> void {
    double net_price = total_price / 1.23;
    double vat_amount = total_price - net_price;
    
    std::ostringstream receipt;
    receipt << std::fixed << std::setprecision(2);
    
    std::string payload = RECEIPT_HEADER;
    
    payload += "\x1B\x61\x01";   // Center align
    payload += "\x1B\x45\x01";   // Bold on
    payload += "PARAGON FISKALNY\n\n";
    payload += "\x1B\x45\x00";   // Bold off

    payload += "\x1B\x61\x00";   // Left align
    receipt << "Wynajem: " << vehicle_name << "\n";
    receipt << rental_duration_hours << " h x " << price_per_hour << " PLN\n";
    receipt << "                            " << total_price << " A\n"; // Tax bracket A
    receipt << "--------------------------------\n";

    receipt << "SP. OPODATKOWANA A:         " << net_price << "\n";
    receipt << "PODATEK PTU 23% A:          " << vat_amount << "\n";
    receipt << "SUMA PTU:                   " << vat_amount << "\n";
    receipt << "--------------------------------\n";
    
    payload += receipt.str();
    
    payload += "\x1B\x61\x01";   // Center align
    payload += "\x1B\x45\x01";   // Bold on
    payload += "\x1D\x21\x11";   // Double Height/Width
    
    std::ostringstream final_total;
    final_total << std::fixed << std::setprecision(2) << "DO ZAPLATY: " << total_price << " PLN\n";
    payload += final_total.str();
    
    payload += "\x1D\x21\x00";   // Normal text
    payload += "\x1B\x45\x00";   // Bold off
    
    payload += RECEIPT_FOOTER;
    
    sendCommand(payload);
}