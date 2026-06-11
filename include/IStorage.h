#pragma once

#include "Vehicle.h"
#include "Types.h"
#include <vector>
#include <unordered_map>

class IStorage {
public:
    IStorage() = default;
    virtual ~IStorage() = default;
    
    IStorage(const IStorage&) = delete;
    auto operator=(const IStorage&) -> IStorage& = delete;
    IStorage(IStorage&&) = delete;
    auto operator=(IStorage&&) -> IStorage& = delete;

    auto virtual loadFleet() -> std::vector<Vehicle> = 0;
    auto virtual saveFleet(const std::vector<Vehicle>& fleet) -> void = 0;

    auto virtual appendRecord(const Record& record) -> void = 0;
    auto virtual getRecordRange(uint32_t vehicle_id, size_t offset, size_t limit) -> std::vector<Record> = 0;
    auto virtual deleteRecords(uint32_t vehicle_id) -> void = 0;

    auto virtual saveActiveRentals(const std::unordered_map<std::string, RentalSession>& rentals) -> void = 0;
    auto virtual loadActiveRentals() -> std::unordered_map<std::string, RentalSession> = 0;
};