#pragma once

#include "IStorage.h"
#include "IPrinter.h"
#include "Exceptions.h"
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>

class MockStorage : public IStorage {
public:
    std::vector<Vehicle> fake_db;
    std::vector<Record> fake_records;

    bool throw_on_load{false};
    bool throw_on_save{false};
    bool throw_on_append{false};
    int append_delay_ms{0};

    std::mutex mtx;

    auto loadFleet() -> std::vector<Vehicle> override {
        if (throw_on_load) throw std::runtime_error("MockStorage: load failure");
        std::scoped_lock lock(mtx);
        return fake_db;
    }

    auto saveFleet(const std::vector<Vehicle>& fleet) -> void override {
        if (throw_on_save) throw std::runtime_error("MockStorage: save failure");
        std::scoped_lock lock(mtx);
        fake_db = fleet;
    }
    
    auto appendRecord(const Record& record) -> void override {
        if (throw_on_append) throw std::runtime_error("MockStorage: append failure");
        if (append_delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(append_delay_ms));
        std::scoped_lock lock(mtx);
        fake_records.push_back(record);
    }

    auto getRecordRange(uint32_t, size_t, size_t) -> std::vector<Record> override {
        std::scoped_lock lock(mtx);
        return fake_records;
    }

    auto deleteRecords(uint32_t vehicle_id) -> void override {
        std::scoped_lock lock(mtx);
        std::erase_if(fake_records, [vehicle_id](const Record& r) {
            return r.vehicle_id == vehicle_id;
        });
    }

    std::unordered_map<std::string, RentalSession> fake_rentals;

    auto saveActiveRentals(const std::unordered_map<std::string, RentalSession>& rentals) -> void override {
        std::scoped_lock lock(mtx);
        fake_rentals = rentals;
    }

    auto loadActiveRentals() -> std::unordered_map<std::string, RentalSession> override {
        std::scoped_lock lock(mtx);
        return fake_rentals;
    }
};

class MockPrinter : public IPrinter {
public:
    int checkout_prints = 0;
    int return_prints = 0;

    bool throw_on_print{false};

    auto printCheckout(const std::string& name, const std::string& code,
                       const std::string& renter_name = "", const std::string& renter_surname = "",
                       const std::string& renter_id = "",
                       std::chrono::system_clock::time_point timestamp = {}) -> void override {
        if (throw_on_print) throw PrinterException("MockPrinter: print failure");
        (void)name; (void)code; (void)renter_name; (void)renter_surname; (void)renter_id; (void)timestamp;
        ++checkout_prints;
    }
    auto printReturn(const std::string& name, int pph, double hours, int total) -> void override {
        if (throw_on_print) throw PrinterException("MockPrinter: print failure");
        (void)name; (void)pph; (void)hours; (void)total;
        ++return_prints;
    }
};