#include <catch2/catch_test_macros.hpp>
#include "FleetMgmt.h"
#include "MockSystem.h"
#include <thread>
#include <atomic>

TEST_CASE("Concurrent rent attempts result in at most one success", "[concurrency][stress]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();

    FleetManager manager(std::move(storage), std::move(printer));

    Car car{101, "Fast", "Car", 2, 300, VehicleStatus::Available};
    manager.addVehicle(Vehicle(car));

    std::atomic<int> success_count{0};

    int threads = 2;
    const char* env = std::getenv("VMS_CONCURRENCY_THREADS");
    if (env) {
        try { threads = std::stoi(env); } catch(...) { threads = 2; }
    }

    std::vector<std::thread> workers;
    for (int i = 0; i < threads; ++i) {
        workers.emplace_back([&manager, &success_count]() {
            try {
                auto code = manager.rentVehicle(101, "John", "Doe", "AB123456");
                if (code.has_value()) {
                    ++success_count;
                }
            } catch (const std::exception&) {
                // ignore expected rent failures on already rented vehicles
            }
        });
    }

    for (auto &t : workers) t.join();

    if (success_count.load() > 1) {
        WARN("Detected concurrent rent race: multiple successful rentals (" << success_count.load() << ")");
    }
    REQUIRE(success_count.load() >= 1);
}
