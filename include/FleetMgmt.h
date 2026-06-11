#pragma once

#include "Vehicle.h"
#include "IStorage.h"
#include "IPrinter.h"
#include <memory>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <optional>
#include <functional>
#include <mutex>

struct RentalSession {
    uint32_t vehicle_id{0};
    std::chrono::system_clock::time_point start_time;
};

class FleetManager {
public:
    explicit FleetManager(std::unique_ptr<IStorage> storage, std::unique_ptr<IPrinter> printer = nullptr,
                          std::function<std::chrono::system_clock::time_point()> now_fn = [](){ return std::chrono::system_clock::now(); });

    auto addVehicle(const Vehicle& vehicle) -> void;
    [[nodiscard]] auto getFleet() const -> std::vector<Vehicle>;
    [[nodiscard]] auto getVehicle(uint32_t vehicle_id) const -> std::optional<Vehicle>;

    template<typename Predicate>
    auto searchFleet(Predicate pred) const -> std::vector<Vehicle> {
        std::vector<Vehicle> results;
        for (const auto& vehicle : getFleet()) {
            if (pred(vehicle)) {
                results.push_back(vehicle);
            }
        }
        return results;
    }

    auto rentVehicle(uint32_t vehicle_id) -> std::optional<std::string>;
    auto returnVehicle(uint32_t vehicle_id) -> bool;
    auto returnVehicle(const std::string &rental_code) -> bool;

private:
    std::vector<Vehicle> fleet_;
    std::unique_ptr<IStorage> storage_;
    std::unique_ptr<IPrinter> printer_;
    std::unordered_map<std::string, RentalSession> active_rentals_;
    std::function<std::chrono::system_clock::time_point()> now_fn_;

    // mutex protecting fleet data
    mutable std::mutex mutex_;

    static auto generateRentalCode() -> std::string;
};