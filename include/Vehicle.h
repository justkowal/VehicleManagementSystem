#pragma once

#include "Types.h"
#include <string>
#include <variant>

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
    [[nodiscard]] auto getPricePerHour() const -> double;

    auto setStatus(VehicleStatus new_status) -> void;

    [[nodiscard]] auto calculateRentalCost(uint32_t hours) const -> double;

    [[nodiscard]] auto getVariant() const -> const VehicleVariant&;
    auto getVariant() -> VehicleVariant&;

private:
    VehicleVariant storage_;
};