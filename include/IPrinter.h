#pragma once

#include <string>

class IPrinter {
public:
    IPrinter() = default;
    virtual ~IPrinter() = default;

    IPrinter(const IPrinter&) = delete;
    auto operator=(const IPrinter&) -> IPrinter& = delete;
    IPrinter(IPrinter&&) = delete;
    auto operator=(IPrinter&&) -> IPrinter& = delete;
    
    virtual auto printCheckout(const std::string& vehicle_name, const std::string& code) -> void = 0;
    virtual auto printReturn(const std::string& vehicle_name, double price_per_hour, double rental_duration_hours, double total_price) -> void = 0;
};