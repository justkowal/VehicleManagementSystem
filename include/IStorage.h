#pragma once

#include "Vehicle.h"
#include <vector>

class IStorage {
public:
    virtual ~IStorage() = default;
    
    IStorage(const IStorage&) = delete;
    auto operator=(const IStorage&) -> IStorage& = delete;
    IStorage(IStorage&&) = delete;
    auto operator=(IStorage&&) -> IStorage& = delete;

    auto virtual loadFleet() -> std::vector<Vehicle> = 0;
    auto virtual saveFleet(const std::vector<Vehicle>& fleet) -> void = 0;

    auto virtual appendRecord(const Record& record) -> void = 0;
    auto virtual getRecordRange(uint32_t vehicle_id, size_t offset, size_t limit) -> std::vector<Record> = 0;
};