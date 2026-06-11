#pragma once

#include <chrono>
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
    int price_per_hour{0};
    VehicleStatus status{VehicleStatus::Available};
};

struct Bike {
    uint32_t id{0};
    std::string brand;
    std::string type;
    int price_per_hour{0};
    VehicleStatus status{VehicleStatus::Available};
};

struct Truck {
    uint32_t id{0};
    std::string brand;
    std::string model;
    uint32_t payload_capacity_kg{0};
    int price_per_hour{0};
    VehicleStatus status{VehicleStatus::Available};
};

enum class RecordType : uint8_t {
    Checkout,
    Returned,
    Maintenance,
    Note
};

struct Record {
    uint32_t vehicle_id;
    std::chrono::system_clock::time_point timestamp;
    RecordType type;
    std::string details;
};

struct RentalSession {
    uint32_t vehicle_id{0};
    std::chrono::system_clock::time_point start_time;
};