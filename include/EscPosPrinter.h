#pragma once

#include "IPrinter.h"
#include <string>

class EscPosPrinter : public IPrinter {
public:
    explicit EscPosPrinter(std::string device_path);
    ~EscPosPrinter() override = default;

    auto printCheckout(const std::string& vehicle_name, const std::string& code) -> void override;
    auto printReturn(const std::string& vehicle_name, double price_per_hour, double rental_duration_hours, double total_price) -> void override;

private:
    std::string device_path_;
    auto sendCommand(const std::string& payload) const -> void;
};