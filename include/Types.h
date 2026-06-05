#pragma once

#include <cstdint>
#include <string>

enum class VehicleStatus : uint8_t {
    Available,
    Rented,
    Maintenance
};

struct Car {
    uint32_t id{0};
    std::string brand;
    std::string model;
    uint8_t seats{0};
    double price_per_hour{0.0};
    VehicleStatus status{VehicleStatus::Available};
};

struct Bike {
    uint32_t id{0};
    std::string brand;
    std::string type;
    double price_per_hour{0.0};
    VehicleStatus status{VehicleStatus::Available};
};

struct Truck {
    uint32_t id{0};
    std::string brand;
    std::string model;
    uint32_t payload_capacity_kg{0};
    double price_per_hour{0.0};
    VehicleStatus status{VehicleStatus::Available};
};