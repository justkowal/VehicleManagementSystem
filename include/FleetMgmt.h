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
#include <shared_mutex>


class FleetManager {
public:
    explicit FleetManager(std::unique_ptr<IStorage> storage, std::unique_ptr<IPrinter> printer = nullptr,
                          std::function<std::chrono::system_clock::time_point()> now_fn = [](){ return std::chrono::system_clock::now(); });

    auto addVehicle(const Vehicle& vehicle) -> void;
    auto removeVehicle(uint32_t vehicle_id) -> bool;
    auto updateVehicleStatus(uint32_t vehicle_id, VehicleStatus status, const std::string& notes = "") -> bool;
    [[nodiscard]] auto getFleet() const -> std::vector<Vehicle>;
    [[nodiscard]] auto getVehicle(uint32_t vehicle_id) const -> std::optional<Vehicle>;

    template<typename Predicate>
    auto searchFleet(Predicate pred, const std::optional<AdvancedFilter>& adv_filter = std::nullopt) const -> std::vector<Vehicle> {
        std::shared_lock lock(mutex_);
        std::vector<Vehicle> results;
        for (const auto& vehicle : fleet_) {
            if (pred(vehicle)) {
                if (!adv_filter.has_value()) {
                    results.push_back(vehicle);
                } else {
                    std::optional<std::string> rental_code = std::nullopt;
                    for (const auto& [code, session] : active_rentals_) {
                        if (session.vehicle_id == vehicle.getId()) {
                            rental_code = code;
                            break;
                        }
                    }
                    if (adv_filter->matches(vehicle, rental_code)) {
                        results.push_back(vehicle);
                    }
                }
            }
        }
        return results;
    }

    auto rentVehicle(uint32_t vehicle_id, const std::string& name = "", const std::string& surname = "", const std::string& id_card = "", std::string* out_print_warning = nullptr) -> std::optional<std::string>;
    auto returnVehicle(uint32_t vehicle_id, int* out_cost = nullptr, const std::string& notes = "", std::string* out_print_warning = nullptr) -> bool;
    auto returnVehicle(const std::string &rental_code, int* out_cost = nullptr, const std::string& notes = "", std::string* out_print_warning = nullptr) -> bool;
    [[nodiscard]] auto getVehicleIdByRentalCode(const std::string& rental_code) const -> std::optional<uint32_t>;
    [[nodiscard]] auto getRentalCodeByVehicleId(uint32_t vehicle_id) const -> std::optional<std::string>;
    [[nodiscard]] auto getRentalBilledHours(uint32_t vehicle_id) const -> std::optional<uint32_t>;

private:
    std::vector<Vehicle> fleet_;
    std::unique_ptr<IStorage> storage_;
    std::unique_ptr<IPrinter> printer_;
    std::unordered_map<std::string, RentalSession> active_rentals_;
    std::function<std::chrono::system_clock::time_point()> now_fn_;

    mutable std::shared_mutex mutex_;

    static auto generateRentalCode() -> std::string;
};