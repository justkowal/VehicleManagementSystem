#include "IPrinter.h"
#include <fstream>
#include <iomanip>
#include <iostream>

class CustomPrinterPlugin : public IPrinter {
  public:
    explicit CustomPrinterPlugin(std::string filepath) : filepath_(std::move(filepath)) {
        if (filepath_.empty() || filepath_ == "127.0.0.1:9100") {
            filepath_ = "receipt.txt";
        }
    }

    auto printCheckout(const std::string& vehicle_name, const std::string& code,
                       const std::string& name, const std::string& surname,
                       const std::string& id_card,
                       std::chrono::system_clock::time_point timestamp) -> void override {
        (void)timestamp;
        std::ofstream out(filepath_, std::ios::app);
        std::ostream& target = out.is_open() ? out : std::cout;

        target << "\n====================================\n";
        target << " [DYNAMIC PLUGIN] VEHICLE RENTED  \n";
        target << " VEHICLE:  " << vehicle_name << "\n";
        target << " AUTH:     " << code << "\n";
        target << " CUSTOMER: " << name << " " << surname << "\n";
        if (!id_card.empty()) {
            target << " ID CARD:  " << id_card << "\n";
        }
        target << "====================================\n\n";
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    auto printReturn(const std::string& vehicle_name, int price_per_hour,
                     double rental_duration_hours, int total_price) -> void override {
        (void)price_per_hour;
        std::ofstream out(filepath_, std::ios::app);
        std::ostream& target = out.is_open() ? out : std::cout;

        target << "\n====================================\n";
        target << " [DYNAMIC PLUGIN] VEHICLE RETURNED     \n";
        target << " VEHICLE:  " << vehicle_name << "\n";
        target << " DURATION: " << std::fixed << std::setprecision(1) << rental_duration_hours
               << " Hours\n";
        target << " COST:     " << (total_price / 100.0) << " PLN\n";
        target << "====================================\n\n";
    }

  private:
    std::string filepath_;
};

extern "C" {
auto create_printer(const char* filepath) -> IPrinter* {
    return new CustomPrinterPlugin(filepath != nullptr ? filepath : "");
}
auto destroy_printer(IPrinter* printer) -> void {
    delete printer;
}
}
