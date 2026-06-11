#pragma once

#include <string>
#include <chrono>

class IPrinter {
public:
    IPrinter() = default;
    virtual ~IPrinter() = default;

    IPrinter(const IPrinter&) = delete;
    auto operator=(const IPrinter&) -> IPrinter& = delete;
    IPrinter(IPrinter&&) = delete;
    auto operator=(IPrinter&&) -> IPrinter& = delete;
    
    virtual auto printCheckout(const std::string& vehicle_name, const std::string& code,
                               const std::string& name = "", const std::string& surname = "",
                               const std::string& id_card = "",
                               std::chrono::system_clock::time_point timestamp = {}) -> void = 0;
    virtual auto printReturn(const std::string& vehicle_name, int price_per_hour, double rental_duration_hours, int total_price) -> void = 0;
};