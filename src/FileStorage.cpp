#include "FileStorage.h"
#include <fstream>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <optional>
#include <iomanip>
#include <ctime>

// Local helpers for parsing/formatting
namespace {
    static std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> out;
        std::string cur;
        std::istringstream ss(s);
        while (std::getline(ss, cur, delim)) {
            out.push_back(cur);
        }
        return out;
    }

    static std::string formatTimeISO(const std::chrono::system_clock::time_point& tp) {
        std::time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &tt);
#else
        gmtime_r(&tt, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    static std::optional<std::chrono::system_clock::time_point> parseTimeISO(const std::string& s) {
        std::istringstream iss(s);
        std::tm tm{};
        iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        if (iss.fail()) {
            return std::nullopt;
        }
#ifdef _WIN32
        // Windows doesn't have timegm; _mkgmtime converts tm in UTC to time_t
        std::time_t tt = _mkgmtime(&tm);
#else
        std::time_t tt = timegm(&tm);
#endif
        return std::chrono::system_clock::from_time_t(tt);
    }
}

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
    const auto& var = v.getVariant();
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
    }, var);
}
auto FileStorage::deserializeVehicle(const std::string& line) const -> std::optional<Vehicle> {
    try {
        auto parts = split(line, ',');
        if (parts.empty()) { return std::nullopt; }
        const std::string& t = parts[0];
        if (t == "Car") {
            if (parts.size() < 7) return std::nullopt;
            Car c{};
            c.id = static_cast<uint32_t>(std::stoul(parts[1]));
            c.brand = parts[2];
            c.model = parts[3];
            c.seats = static_cast<uint8_t>(std::stoul(parts[4]));
            c.price_per_hour = std::stod(parts[5]);
            c.status = static_cast<VehicleStatus>(std::stoul(parts[6]));
            return Vehicle{c};
        } else if (t == "Bike") {
            if (parts.size() < 6) return std::nullopt;
            Bike b{};
            b.id = static_cast<uint32_t>(std::stoul(parts[1]));
            b.brand = parts[2];
            b.type = parts[3];
            b.price_per_hour = std::stod(parts[4]);
            b.status = static_cast<VehicleStatus>(std::stoul(parts[5]));
            return Vehicle{b};
        } else if (t == "Truck") {
            if (parts.size() < 7) return std::nullopt;
            Truck tr{};
            tr.id = static_cast<uint32_t>(std::stoul(parts[1]));
            tr.brand = parts[2];
            tr.model = parts[3];
            tr.payload_capacity_kg = static_cast<uint32_t>(std::stoul(parts[4]));
            tr.price_per_hour = std::stod(parts[5]);
            tr.status = static_cast<VehicleStatus>(std::stoul(parts[6]));
            return Vehicle{tr};
        }
    } catch (const std::exception& e) {
        std::cerr << "deserializeVehicle parse error: " << e.what() << "\n";
        return std::nullopt;
    }
    return std::nullopt;
}
auto FileStorage::serializeRecord(const Record& r) const -> std::string {
    std::ostringstream oss;
    oss << r.vehicle_id << ',' << formatTimeISO(r.timestamp) << ',' << static_cast<int>(r.type) << ',' << r.details;
    return oss.str();
}
auto FileStorage::deserializeRecord(const std::string& line) const -> std::optional<Record> {
    try {
        // Parse first three comma-separated fields; details may contain commas so take the rest
        size_t pos1 = line.find(',');
        if (pos1 == std::string::npos) return std::nullopt;
        size_t pos2 = line.find(',', pos1 + 1);
        if (pos2 == std::string::npos) return std::nullopt;
        size_t pos3 = line.find(',', pos2 + 1);
        if (pos3 == std::string::npos) return std::nullopt;

        std::string id_str = line.substr(0, pos1);
        std::string ts_str = line.substr(pos1 + 1, pos2 - (pos1 + 1));
        std::string type_str = line.substr(pos2 + 1, pos3 - (pos2 + 1));
        std::string details = line.substr(pos3 + 1);

        Record rec{};
        rec.vehicle_id = static_cast<uint32_t>(std::stoul(id_str));
        auto tp_opt = parseTimeISO(ts_str);
        if (!tp_opt) return std::nullopt;
        rec.timestamp = *tp_opt;
        int type_int = std::stoi(type_str);
        if (type_int < 0 || type_int > static_cast<int>(RecordType::Note)) return std::nullopt;
        rec.type = static_cast<RecordType>(type_int);
        rec.details = details;
        return rec;
    } catch (const std::exception& e) {
        std::cerr << "deserializeRecord parse error: " << e.what() << "\n";
        return std::nullopt;
    }
}