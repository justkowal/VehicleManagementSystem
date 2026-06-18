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

    Car car{1, "X", "Y", 4, 500, VehicleStatus::Available};
    Vehicle v(car);

    raw->throw_on_save = true;
    REQUIRE_THROWS_AS(manager.addVehicle(v), std::runtime_error);
}

TEST_CASE("Transactional integrity: rentVehicle rollback on storage failure", "[fleet][transaction]") {
    auto storage = std::make_unique<MockStorage>();
    auto* raw_storage = storage.get();
    auto printer = std::make_unique<MockPrinter>();

    FleetManager manager(std::move(storage), std::move(printer));

    Car car{101, "Tesla", "Model S", 5, 2500, VehicleStatus::Available};
    manager.addVehicle(Vehicle(car));

    raw_storage->throw_on_save = true;

    REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "Doe", "AB123456"), std::runtime_error);

    REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Available);
}

TEST_CASE("Printer failure does not roll back database transaction", "[fleet][transaction]") {
    auto storage = std::make_unique<MockStorage>();
    auto* raw_storage = storage.get();
    auto printer = std::make_unique<MockPrinter>();
    auto* raw_printer = printer.get();

    FleetManager manager(std::move(storage), std::move(printer));

    Car car{101, "Tesla", "Model S", 5, 2500, VehicleStatus::Available};
    manager.addVehicle(Vehicle(car));

    raw_printer->throw_on_print = true;

    std::string print_warning;
    auto rental_code = manager.rentVehicle(101, "John", "Doe", "AB123456", &print_warning);

    REQUIRE(rental_code.has_value() == true);
    REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Rented);
    REQUIRE(raw_storage->fake_db[0].getStatus() == VehicleStatus::Rented);

    REQUIRE(print_warning.empty() == false);
    REQUIRE(print_warning.find("MockPrinter: print failure") != std::string::npos);
}
