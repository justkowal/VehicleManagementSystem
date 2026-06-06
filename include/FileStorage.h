#pragma once

#include "IStorage.h"
#include <string>

class FileStorage : public IStorage {
public:
    explicit FileStorage(std::string base_path);
    ~FileStorage() override = default;

    FileStorage(const FileStorage&) = delete;
    auto operator=(const FileStorage&) -> FileStorage& = delete;
    FileStorage(FileStorage&&) = delete;
    auto operator=(FileStorage&&) -> FileStorage& = delete;

    [[nodiscard]] auto loadFleet() -> std::vector<Vehicle> override;
    auto saveFleet(const std::vector<Vehicle>& fleet) -> void override;

    auto appendRecord(const Record& record) -> void override;
    [[nodiscard]] auto getRecordRange(uint32_t vehicle_id, size_t offset, size_t limit) -> std::vector<Record> override;

private:
    std::string base_path_;

    [[nodiscard]] auto serializeVehicle(const Vehicle& vehicle) const -> std::string;
    [[nodiscard]] auto deserializeVehicle(const std::string& line) const -> std::optional<Vehicle>;
    
    [[nodiscard]] auto serializeRecord(const Record& record) const -> std::string;
    [[nodiscard]] auto deserializeRecord(const std::string& line) const -> std::optional<Record>;
};