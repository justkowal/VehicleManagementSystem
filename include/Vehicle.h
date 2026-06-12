#pragma once

#include "Types.h"
#include <string>
#include <variant>
#include <optional>
#include <cmath>

class Vehicle {
public:
    using VehicleVariant = std::variant<Car, Bike, Truck>;

    explicit Vehicle(Car car);
    explicit Vehicle(Bike bike);
    explicit Vehicle(Truck truck);

    [[nodiscard]] auto getId() const -> uint32_t;
    [[nodiscard]] auto getBrand() const -> std::string;
    [[nodiscard]] auto getModelOrType() const -> std::string;
    [[nodiscard]] auto getTypeString() const -> std::string;
    [[nodiscard]] auto getStatus() const -> VehicleStatus;
    [[nodiscard]] auto getStatusString() const -> std::string;
    [[nodiscard]] auto getPricePerHour() const -> int;

    auto setStatus(VehicleStatus new_status) -> void;

    [[nodiscard]] auto calculateRentalCost(uint32_t hours) const -> int;

    [[nodiscard]] auto getVariant() const -> const VehicleVariant&;
    auto getVariant() -> VehicleVariant&;

private:
    VehicleVariant storage_;
};

struct FilterRequirement {
    std::optional<std::string> min_val;
    std::optional<std::string> max_val;
    std::optional<std::string> equals_val;

    [[nodiscard]] bool active() const {
        return min_val.has_value() || max_val.has_value() || equals_val.has_value();
    }
};

struct AdvancedFilter {
    FilterRequirement id;
    FilterRequirement brand;
    FilterRequirement model;
    FilterRequirement type;
    FilterRequirement status;
    FilterRequirement price;
    FilterRequirement seats;
    FilterRequirement payload;
    FilterRequirement rental_code;

    [[nodiscard]] bool active() const {
        return id.active() || brand.active() || model.active() || type.active() ||
               status.active() || price.active() || seats.active() || payload.active() ||
               rental_code.active();
    }

    [[nodiscard]] bool matches(const Vehicle& vehicle, const std::optional<std::string>& rental_code_val) const {
        auto safe_stoul = [](const std::string& str_val) -> uint32_t {
            try {
                return static_cast<uint32_t>(std::stoul(str_val));
            } catch (...) {
                return 0;
            }
        };
        auto safe_stod_grosz = [](const std::string& str_val) -> int {
            try {
                return static_cast<int>(std::round(std::stod(str_val) * 100.0));
            } catch (...) {
                return 0;
            }
        };

        // id
        if (id.active()) {
            uint32_t val = vehicle.getId();
            if (id.min_val.has_value() && val < safe_stoul(*id.min_val)) {
                return false;
            }
            if (id.max_val.has_value() && val > safe_stoul(*id.max_val)) {
                return false;
            }
            if (id.equals_val.has_value() && val != safe_stoul(*id.equals_val)) {
                return false;
            }
        }

        // brand
        if (brand.active()) {
            std::string val = vehicle.getBrand();
            if (brand.min_val.has_value() && val < *brand.min_val) {
                return false;
            }
            if (brand.max_val.has_value() && val > *brand.max_val) {
                return false;
            }
            if (brand.equals_val.has_value() && val != *brand.equals_val) {
                return false;
            }
        }

        // model
        if (model.active()) {
            std::string val = vehicle.getModelOrType();
            if (model.min_val.has_value() && val < *model.min_val) {
                return false;
            }
            if (model.max_val.has_value() && val > *model.max_val) {
                return false;
            }
            if (model.equals_val.has_value() && val != *model.equals_val) {
                return false;
            }
        }

        // type
        if (type.active()) {
            std::string val = vehicle.getTypeString();
            if (type.min_val.has_value() && val < *type.min_val) {
                return false;
            }
            if (type.max_val.has_value() && val > *type.max_val) {
                return false;
            }
            if (type.equals_val.has_value() && val != *type.equals_val) {
                return false;
            }
        }

        // status
        if (status.active()) {
            std::string val = vehicle.getStatusString();
            if (status.min_val.has_value() && val < *status.min_val) {
                return false;
            }
            if (status.max_val.has_value() && val > *status.max_val) {
                return false;
            }
            if (status.equals_val.has_value() && val != *status.equals_val) {
                return false;
            }
        }

        // price
        if (price.active()) {
            int val = vehicle.getPricePerHour();
            if (price.min_val.has_value() && val < safe_stod_grosz(*price.min_val)) {
                return false;
            }
            if (price.max_val.has_value() && val > safe_stod_grosz(*price.max_val)) {
                return false;
            }
            if (price.equals_val.has_value() && val != safe_stod_grosz(*price.equals_val)) {
                return false;
            }
        }

        // seats (only for car)
        if (seats.active()) {
            const auto& variant = vehicle.getVariant();
            if (!std::holds_alternative<Car>(variant)) {
                return false;
            }
            uint8_t val = std::get<Car>(variant).seats;
            if (seats.min_val.has_value() && val < safe_stoul(*seats.min_val)) {
                return false;
            }
            if (seats.max_val.has_value() && val > safe_stoul(*seats.max_val)) {
                return false;
            }
            if (seats.equals_val.has_value() && val != safe_stoul(*seats.equals_val)) {
                return false;
            }
        }

        // payload capacity (only for truck)
        if (payload.active()) {
            const auto& variant = vehicle.getVariant();
            if (!std::holds_alternative<Truck>(variant)) {
                return false;
            }
            uint32_t val = std::get<Truck>(variant).payload_capacity_kg;
            if (payload.min_val.has_value() && val < safe_stoul(*payload.min_val)) {
                return false;
            }
            if (payload.max_val.has_value() && val > safe_stoul(*payload.max_val)) {
                return false;
            }
            if (payload.equals_val.has_value() && val != safe_stoul(*payload.equals_val)) {
                return false;
            }
        }

        // rental code
        if (rental_code.active()) {
            std::string val = rental_code_val.value_or("");
            if (rental_code.min_val.has_value() && val < *rental_code.min_val) {
                return false;
            }
            if (rental_code.max_val.has_value() && val > *rental_code.max_val) {
                return false;
            }
            if (rental_code.equals_val.has_value() && val != *rental_code.equals_val) {
                return false;
            }
        }

        return true;
    }
};