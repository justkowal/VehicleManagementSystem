#include "FileStorage.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

FileStorage::FileStorage(std::string base_path) : base_path_(std::move(base_path)) {
    // Bootstrap the database folders on launch
    fs::create_directories(base_path_ + "/records");
}

auto FileStorage::saveFleet(const std::vector<Vehicle>& fleet) -> void {
    std::ofstream file(base_path_ + "/fleet.txt", std::ios::trunc);
    if (!file.is_open()) { return;
}

    for (const auto& vehicle : fleet) {
        // Uses the compile-time visitor to get the CSV string
        file << serializeVehicle(vehicle) << "\n";
    }
}

auto FileStorage::loadFleet() -> std::vector<Vehicle> {
    std::vector<Vehicle> fleet;
    std::ifstream file(base_path_ + "/fleet.txt");
    if (!file.is_open()) { return fleet;
}

    std::string line;
    while (std::getline(file, line)) {
        if (auto vehicle = deserializeVehicle(line)) {
            fleet.push_back(std::move(*vehicle));
        }
    }
    return fleet;
}

auto FileStorage::appendRecord(const Record& record) -> void {
    std::string filepath = base_path_ + "/records/" + std::to_string(record.vehicle_id) + ".txt";
    std::ofstream file(filepath, std::ios::app); // APPEND MODE
    
    if (file.is_open()) {
        file << serializeRecord(record) << "\n";
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto FileStorage::getRecordRange(uint32_t vehicle_id, size_t offset, size_t limit) -> std::vector<Record> {
    std::string filepath = base_path_ + "/records/" + std::to_string(vehicle_id) + ".txt";
    std::ifstream file(filepath);
    
    std::vector<Record> all_records;
    if (!file.is_open()) { return all_records;
}

    std::string line;
    while (std::getline(file, line)) {
        if (auto rec = deserializeRecord(line)) {
            all_records.push_back(std::move(*rec));
        }
    }

    // Reverse to get newest first
    std::reverse(all_records.begin(), all_records.end());

    // Slice for pagination
    if (offset >= all_records.size()) {
        return {};
    }
    size_t end_idx = std::min(offset + limit, all_records.size());

    // Avoid narrowing conversion warnings when adding size_t to iterator's difference_type
    using diff_t = std::vector<Record>::difference_type;
    auto it_begin = all_records.begin() + static_cast<diff_t>(offset);
    auto it_end = all_records.begin() + static_cast<diff_t>(end_idx);

    return std::vector<Record>(it_begin, it_end);
}

// --- Serializer Stubs (To be filled with standard string splitting/joining) ---
auto FileStorage::serializeVehicle(const Vehicle& v) const -> std::string {
    // TODO: implement std::visit to turn variant into "Car,101,Toyota,Yaris,5,15.5,0"
    return ""; 
}
auto FileStorage::deserializeVehicle(const std::string& line) const -> std::optional<Vehicle> {
    // TODO: split string, check first token ("Car"), build struct, return Vehicle(car)
    return std::nullopt;
}
auto FileStorage::serializeRecord(const Record& r) const -> std::string {
    // TODO: convert chrono to string, append enum and details
    return "";
}
auto FileStorage::deserializeRecord(const std::string& line) const -> std::optional<Record> {
    // TODO: parse string back into Record struct
    return std::nullopt;
}