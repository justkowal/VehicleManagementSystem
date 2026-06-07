#include "FileStorage.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <optional>
#include <iomanip>
#include <ctime>

// helpers
namespace {
    auto splitString(const std::string& input, char delim) -> std::vector<std::string> {
        std::vector<std::string> out;
        std::string token;
        std::istringstream iss(input);
        while (std::getline(iss, token, delim)) {
            out.push_back(token);
        }
        return out;
    }

    auto formatTimeISO(const std::chrono::system_clock::time_point& timepoint) -> std::string {
        std::time_t time_t_val = std::chrono::system_clock::to_time_t(timepoint);
        std::tm tm_struct{};
#ifdef _WIN32
        gmtime_s(&tm_struct, &time_t_val);
#else
        gmtime_r(&time_t_val, &tm_struct);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm_struct, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    auto parseTimeISO(const std::string& time_str) -> std::optional<std::chrono::system_clock::time_point> {
        std::istringstream iss(time_str);
        std::tm tm_struct{};
        iss >> std::get_time(&tm_struct, "%Y-%m-%dT%H:%M:%SZ");
        if (iss.fail()) {
            return std::nullopt;
        }
#ifdef _WIN32
    // windows: use _mkgmtime
    std::time_t time_t_val = _mkgmtime(&tm_struct);
#else
        std::time_t time_t_val = timegm(&tm_struct);
#endif
        return std::chrono::system_clock::from_time_t(time_t_val);
    }
}

namespace fs = std::filesystem;

FileStorage::FileStorage(std::string base_path) : base_path_(std::move(base_path)) {
    // ensure dirs
    fs::create_directories(base_path_ + "/records");
}

auto FileStorage::saveFleet(const std::vector<Vehicle>& fleet) -> void {
    std::ofstream file(base_path_ + "/fleet.txt", std::ios::trunc);
    if (!file.is_open()) {
        return;
    }

    for (const auto& vehicle : fleet) {
        file << serializeVehicle(vehicle) << "\n";
    }
}

auto FileStorage::loadFleet() -> std::vector<Vehicle> {
    std::vector<Vehicle> fleet;
    std::ifstream file(base_path_ + "/fleet.txt");
    if (!file.is_open()) {
        return fleet;
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
    std::ofstream file(filepath, std::ios::app); // append
    
    if (file.is_open()) {
        file << serializeRecord(record) << "\n";
    }
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto FileStorage::getRecordRange(uint32_t vehicle_id, size_t offset, size_t limit) -> std::vector<Record> {
    std::string filepath = base_path_ + "/records/" + std::to_string(vehicle_id) + ".txt";
    std::ifstream file(filepath);
    
    std::vector<Record> all_records;
    if (!file.is_open()) {
        return all_records;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (auto rec = deserializeRecord(line)) {
            all_records.push_back(std::move(*rec));
        }
    }

    // newest first
    std::reverse(all_records.begin(), all_records.end());

    // paginate
    if (offset >= all_records.size()) {
        return {};
    }
    size_t end_idx = std::min(offset + limit, all_records.size());

    // avoid narrowing
    using diff_t = std::vector<Record>::difference_type;
    auto it_begin = all_records.begin() + static_cast<diff_t>(offset);
    auto it_end = all_records.begin() + static_cast<diff_t>(end_idx);

    return std::vector<Record>(it_begin, it_end);
}

// serializers
auto FileStorage::serializeVehicle(const Vehicle& vehicle) -> std::string {
    const auto& variant = vehicle.getVariant();
    return std::visit([](auto&& obj) -> std::string {
        using T = std::decay_t<decltype(obj)>;
        std::ostringstream oss;
        if constexpr (std::is_same_v<T, Car>) {
            oss << "Car," << obj.id << ',' << obj.brand << ',' << obj.model << ','
                << static_cast<int>(obj.seats) << ',' << obj.price_per_hour << ','
                << static_cast<int>(obj.status);
        } else if constexpr (std::is_same_v<T, Bike>) {
            oss << "Bike," << obj.id << ',' << obj.brand << ',' << obj.type << ','
                << obj.price_per_hour << ',' << static_cast<int>(obj.status);
        } else if constexpr (std::is_same_v<T, Truck>) {
            oss << "Truck," << obj.id << ',' << obj.brand << ',' << obj.model << ','
                << obj.payload_capacity_kg << ',' << obj.price_per_hour << ','
                << static_cast<int>(obj.status);
        }
        return oss.str();
    }, variant);
}
auto FileStorage::deserializeVehicle(const std::string& line) -> std::optional<Vehicle> {
    try {
        auto parts = splitString(line, ',');
        if (parts.empty()) {
            return std::nullopt;
        }
        const std::string& type_token = parts[0];
        if (type_token == "Car") {
            if (parts.size() < 7) {
                return std::nullopt;
            }
            Car car{};
            car.id = static_cast<uint32_t>(std::stoul(parts[1]));
            car.brand = parts[2];
            car.model = parts[3];
            car.seats = static_cast<uint8_t>(std::stoul(parts[4]));
            car.price_per_hour = std::stod(parts[5]);
            car.status = static_cast<VehicleStatus>(std::stoul(parts[6]));
            return Vehicle{car};
        }
        if (type_token == "Bike") {
            if (parts.size() < 6) {
                return std::nullopt;
            }
            Bike bike{};
            bike.id = static_cast<uint32_t>(std::stoul(parts[1]));
            bike.brand = parts[2];
            bike.type = parts[3];
            bike.price_per_hour = std::stod(parts[4]);
            bike.status = static_cast<VehicleStatus>(std::stoul(parts[5]));
            return Vehicle{bike};
        }
        if (type_token == "Truck") {
            if (parts.size() < 7) {
                return std::nullopt;
            }
            Truck truck_var{};
            truck_var.id = static_cast<uint32_t>(std::stoul(parts[1]));
            truck_var.brand = parts[2];
            truck_var.model = parts[3];
            truck_var.payload_capacity_kg = static_cast<uint32_t>(std::stoul(parts[4]));
            truck_var.price_per_hour = std::stod(parts[5]);
            truck_var.status = static_cast<VehicleStatus>(std::stoul(parts[6]));
            return Vehicle{truck_var};
        }
    } catch (const std::exception& e) {
        std::cerr << "deserializeVehicle parse error: " << e.what() << "\n";
        return std::nullopt;
    }
    return std::nullopt;
}
auto FileStorage::serializeRecord(const Record& record) -> std::string {
    std::ostringstream oss;
    oss << record.vehicle_id << ',' << formatTimeISO(record.timestamp) << ',' << static_cast<int>(record.type) << ',' << record.details;
    return oss.str();
}
auto FileStorage::deserializeRecord(const std::string& line) -> std::optional<Record> {
    try {
        // parse first 3 fields; rest details
        size_t pos1 = line.find(',');
        if (pos1 == std::string::npos) {
            return std::nullopt;
        }
        size_t pos2 = line.find(',', pos1 + 1);
        if (pos2 == std::string::npos) {
            return std::nullopt;
        }
        size_t pos3 = line.find(',', pos2 + 1);
        if (pos3 == std::string::npos) {
            return std::nullopt;
        }

        std::string id_str = line.substr(0, pos1);
        std::string ts_str = line.substr(pos1 + 1, pos2 - (pos1 + 1));
        std::string type_str = line.substr(pos2 + 1, pos3 - (pos2 + 1));
        std::string details = line.substr(pos3 + 1);

        Record rec{};
        rec.vehicle_id = static_cast<uint32_t>(std::stoul(id_str));
        auto time_opt = parseTimeISO(ts_str);
        if (!time_opt) {
            return std::nullopt;
        }
        rec.timestamp = *time_opt;
        int type_int = std::stoi(type_str);
        if (type_int < 0 || type_int > static_cast<int>(RecordType::Note)) {
            return std::nullopt;
        }
        rec.type = static_cast<RecordType>(type_int);
        rec.details = details;
        return rec;
    } catch (const std::exception& e) {
        std::cerr << "deserializeRecord parse error: " << e.what() << "\n";
        return std::nullopt;
    }
}