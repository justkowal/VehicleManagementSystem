#include <catch2/catch_test_macros.hpp>
#include "FleetMgmt.h"
#include "MockSystem.h"

TEST_CASE("FleetManager Domain Logic", "[fleet]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();
    
    auto* raw_storage = storage.get();
    auto* raw_printer = printer.get();
    
    FleetManager manager(std::move(storage), std::move(printer));

    Car test_car{101, "Toyota", "Yaris", 5, 15.0, VehicleStatus::Available};

    SECTION("Adding a vehicle saves it to storage immediately") {
        manager.addVehicle(Vehicle(test_car));
        
        REQUIRE(manager.getFleet().size() == 1);
        REQUIRE(raw_storage->fake_db.size() == 1);
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Available);
    }

    SECTION("Renting a vehicle updates state and prints a receipt") {
        manager.addVehicle(Vehicle(test_car));
        
        auto rental_code = manager.rentVehicle(101);
        
        REQUIRE(rental_code.has_value() == true);
        REQUIRE(rental_code->substr(0, 5) == "RENT-");
        
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Rented);
        REQUIRE(raw_storage->fake_db[0].getStatus() == VehicleStatus::Rented);
        
        REQUIRE(raw_printer->checkout_prints == 1);
        REQUIRE(raw_storage->fake_records.size() == 1);
    }

    SECTION("Cannot rent an already rented vehicle") {
        manager.addVehicle(Vehicle(test_car));
        manager.rentVehicle(101);
        
        auto second_attempt = manager.rentVehicle(101);
        
        REQUIRE(second_attempt.has_value() == false);
        REQUIRE(raw_printer->checkout_prints == 1);
    }

    SECTION("Returning a vehicle calculates costs and updates state") {
        manager.addVehicle(Vehicle(test_car));
        auto code = manager.rentVehicle(101);
        
        bool return_success = manager.returnVehicle(code.value());
        
        REQUIRE(return_success == true);
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Available);
        REQUIRE(raw_printer->return_prints == 1);
    }

    SECTION("Returning with an invalid code fails gracefully") {
        manager.addVehicle(Vehicle(test_car));
        manager.rentVehicle(101);
        
        bool return_success = manager.returnVehicle("AUTH-INVALID");
        
        REQUIRE(return_success == false);
        REQUIRE(manager.getVehicle(101)->getStatus() == VehicleStatus::Rented);
        REQUIRE(raw_printer->return_prints == 0);
    }
}