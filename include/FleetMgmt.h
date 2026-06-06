#pragma once

#include "Vehicle.h"
#include "IStorage.h"
#include "IPrinter.h"
#include <memory>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <optional>

struct RentalSession {
    uint32_t vehicle_id;
    std::chrono::system_clock::time_point start_time;
};

class FleetManager {
    explicit FleetManager(std::unique_ptr<IStorage> storage, std::unique_ptr<IPrinter> printer = nullptr);

    auto addVehicle(const Vehicle& vehicle) -> void;
    auto getFleet() const -> std::vector<Vehicle>;
    auto getVehicle(uint32_t id) const -> std::optional<Vehicle>;

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
}