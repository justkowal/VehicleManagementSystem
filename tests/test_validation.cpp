#include <catch2/catch_test_macros.hpp>
#include "Exceptions.h"
#include "FleetMgmt.h"
#include "MockSystem.h"
#include <algorithm>
#include <cctype>

TEST_CASE("Vehicle input validation", "[validation][vehicle]") {
    SECTION("Valid Car does not throw") {
        REQUIRE_NOTHROW(Vehicle(Car{1, "Tesla", "Model S", 5, 2500, VehicleStatus::Available}));
    }
    SECTION("Valid Bike does not throw") {
        REQUIRE_NOTHROW(Vehicle(Bike{2, "Giant", "Escape", 500, VehicleStatus::Available}));
    }
    SECTION("Valid Truck does not throw") {
        REQUIRE_NOTHROW(Vehicle(Truck{3, "Volvo", "FH16", 25000, 5000, VehicleStatus::Available}));
    }

    SECTION("Zero vehicle ID throws ValidationException") {
        REQUIRE_THROWS_AS(Vehicle(Car{0, "Tesla", "Model S", 5, 2500, VehicleStatus::Available}), ValidationException);
        REQUIRE_THROWS_AS(Vehicle(Bike{0, "Giant", "Escape", 500, VehicleStatus::Available}), ValidationException);
        REQUIRE_THROWS_AS(Vehicle(Truck{0, "Volvo", "FH16", 25000, 5000, VehicleStatus::Available}), ValidationException);
    }

    SECTION("Zero/negative rental price throws ValidationException") {
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Tesla", "Model S", 5, 0, VehicleStatus::Available}), ValidationException);
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Tesla", "Model S", 5, -100, VehicleStatus::Available}), ValidationException);
    }

    SECTION("Empty brand/model strings throw ValidationException") {
        REQUIRE_THROWS_AS(Vehicle(Car{1, "", "Model S", 5, 2500, VehicleStatus::Available}), ValidationException);
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Tesla", "", 5, 2500, VehicleStatus::Available}), ValidationException);
    }

    SECTION("Commas in brand/model strings throw ValidationException") {
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Te,sla", "Model S", 5, 2500, VehicleStatus::Available}), ValidationException);
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Tesla", "Model,S", 5, 2500, VehicleStatus::Available}), ValidationException);
    }

    SECTION("Newlines in brand/model strings throw ValidationException") {
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Tesla\n", "Model S", 5, 2500, VehicleStatus::Available}), ValidationException);
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Tesla", "Model\rS", 5, 2500, VehicleStatus::Available}), ValidationException);
    }

    SECTION("Non-printable ASCII in brand/model strings throw ValidationException") {
        std::string bad_str = "Tesla";
        bad_str[2] = 7; 
        REQUIRE_THROWS_AS(Vehicle(Car{1, bad_str, "Model S", 5, 2500, VehicleStatus::Available}), ValidationException);
    }

    SECTION("Zero seats in Car throws ValidationException") {
        REQUIRE_THROWS_AS(Vehicle(Car{1, "Tesla", "Model S", 0, 2500, VehicleStatus::Available}), ValidationException);
    }

    SECTION("Zero payload capacity in Truck throws ValidationException") {
        REQUIRE_THROWS_AS(Vehicle(Truck{3, "Volvo", "FH16", 0, 5000, VehicleStatus::Available}), ValidationException);
    }
}

TEST_CASE("Rent transaction input validation", "[validation][rent]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();
    FleetManager manager(std::move(storage), std::move(printer));

    Car car{101, "Tesla", "Model S", 5, 2500, VehicleStatus::Available};
    manager.addVehicle(Vehicle(car));

    SECTION("Valid renter details work") {
        auto code = manager.rentVehicle(101, "John", "Doe", "AB123456");
        REQUIRE(code.has_value());
    }

    SECTION("Empty first/last name throws ValidationException") {
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "", "Doe", "AB123456"), ValidationException);
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "", "AB123456"), ValidationException);
    }

    SECTION("Non-alphabetic name characters throw ValidationException") {
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John1", "Doe", "AB123456"), ValidationException);
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "Doe-Smith", "AB123456"), ValidationException);
    }

    SECTION("Commas or newlines in name throw ValidationException") {
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John,", "Doe", "AB123456"), ValidationException);
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "Doe\n", "AB123456"), ValidationException);
    }

    SECTION("Empty ID card throws ValidationException") {
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "Doe", ""), ValidationException);
    }

    SECTION("Non-alphanumeric ID card characters throw ValidationException") {
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "Doe", "AB-123456"), ValidationException);
        REQUIRE_THROWS_AS(manager.rentVehicle(101, "John", "Doe", "AB_123"), ValidationException);
    }
}

TEST_CASE("Return transaction and status update input validation", "[validation][return]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();
    FleetManager manager(std::move(storage), std::move(printer));

    Car car{101, "Tesla", "Model S", 5, 2500, VehicleStatus::Available};
    manager.addVehicle(Vehicle(car));

    auto code_opt = manager.rentVehicle(101, "John", "Doe", "AB123456");
    REQUIRE(code_opt.has_value());
    std::string valid_code = code_opt.value();

    SECTION("Valid uppercase rental code works") {
        int cost = 0;
        REQUIRE(manager.returnVehicle(valid_code, &cost, "Clean return"));
    }

    SECTION("Valid lowercase rental code is normalized and works") {
        int cost = 0;
        std::string lowercase_code = valid_code;
        for (char& c : lowercase_code) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        REQUIRE(manager.returnVehicle(lowercase_code, &cost, "Clean return"));
    }

    SECTION("Malformed rental code throws ValidationException") {
        int cost = 0;
        REQUIRE_THROWS_AS(manager.returnVehicle("RENT-12345", &cost, "Notes"), ValidationException);
        REQUIRE_THROWS_AS(manager.returnVehicle("RENT123456", &cost, "Notes"), ValidationException);
        REQUIRE_THROWS_AS(manager.returnVehicle("CODE-123456", &cost, "Notes"), ValidationException);
        REQUIRE_THROWS_AS(manager.returnVehicle("RENT-12345*", &cost, "Notes"), ValidationException);
    }

    SECTION("Comma in return notes throws ValidationException") {
        int cost = 0;
        REQUIRE_THROWS_AS(manager.returnVehicle(valid_code, &cost, "Clean, return"), ValidationException);
    }

    SECTION("Newline in return notes throws ValidationException") {
        int cost = 0;
        REQUIRE_THROWS_AS(manager.returnVehicle(valid_code, &cost, "Clean\nreturn"), ValidationException);
    }

    SECTION("Newline in status update notes throws ValidationException") {
        REQUIRE_THROWS_AS(manager.updateVehicleStatus(101, VehicleStatus::Maintenance, "Broken\nengine"), ValidationException);
    }
}
