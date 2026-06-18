#include <catch2/catch_test_macros.hpp>
#include "FleetMgmt.h"
#include "MockSystem.h"

TEST_CASE("Return handles negative durations from clock adjustments", "[time][anomaly]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();

    std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point current = t0;
    auto clock_fn = [&current]() { return current; };

    FleetManager manager(std::move(storage), std::move(printer), clock_fn);

    Car car{500, "Clocky", "Shift", 2, 400, VehicleStatus::Available};
    manager.addVehicle(Vehicle(car));

    auto code = manager.rentVehicle(500, "John", "Doe", "AB123456");
    REQUIRE(code.has_value());

    current = current - std::chrono::hours(2);

    bool ok = manager.returnVehicle(code.value());
    REQUIRE(ok == true);
    REQUIRE(manager.getVehicle(500)->getStatus() == VehicleStatus::Available);
}
