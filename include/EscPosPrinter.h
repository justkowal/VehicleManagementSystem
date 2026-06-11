#pragma once

#include "IPrinter.h"
#include <string>
#include <chrono>

class EscPosPrinter : public IPrinter {
  public:
    explicit EscPosPrinter(std::string device_path);
    ~EscPosPrinter() override = default;

    EscPosPrinter(const EscPosPrinter&) = delete;
    auto operator=(const EscPosPrinter&) -> EscPosPrinter& = delete;
    EscPosPrinter(EscPosPrinter&&) = delete;
    auto operator=(EscPosPrinter&&) -> EscPosPrinter& = delete;

    auto printCheckout(const std::string& vehicle_name, const std::string& code,
                       const std::string& name = "", const std::string& surname = "",
                       const std::string& id_card = "",
                       std::chrono::system_clock::time_point timestamp = {}) -> void override;
    auto printReturn(const std::string& vehicle_name, int price_per_hour,
                     double rental_duration_hours, int total_price) -> void override;

  private:
    std::string device_path_;
    auto sendCommand(const std::string& payload) const -> void;
};