#include <catch2/catch_test_macros.hpp>
#include "FleetMgmt.h"
#include "MockSystem.h"

TEST_CASE("FleetManager propagates storage save failures", "[storage][failure]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();

    storage->fake_db.clear();
    FleetManager manager(std::move(storage), std::move(printer));
}

TEST_CASE("addVehicle throws if storage save fails", "[storage][failure]") {
    auto storage = std::make_unique<MockStorage>();
    auto* raw = storage.get();
    auto printer = std::make_unique<MockPrinter>();

    FleetManager manager(std::move(storage), std::move(printer));

    Car car{1, "X", "Y", 4, 5.0, VehicleStatus::Available};
    Vehicle v(car);

    raw->throw_on_save = true;
    REQUIRE_THROWS_AS(manager.addVehicle(v), std::runtime_error);
}
