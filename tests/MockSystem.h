#pragma once

#include "IStorage.h"
#include "IPrinter.h"
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
};

class MockPrinter : public IPrinter {
public:
    int checkout_prints = 0;
    int return_prints = 0;

    bool throw_on_print{false};

    auto printCheckout(const std::string& name, const std::string& code) -> void override {
        if (throw_on_print) throw std::runtime_error("MockPrinter: print failure");
        (void)name; (void)code;
        ++checkout_prints;
    }
    auto printReturn(const std::string& name, double pph, double hours, double total) -> void override {
        if (throw_on_print) throw std::runtime_error("MockPrinter: print failure");
        (void)name; (void)pph; (void)hours; (void)total;
        ++return_prints;
    }
};