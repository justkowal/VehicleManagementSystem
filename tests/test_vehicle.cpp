#include <catch2/catch_test_macros.hpp>
#include "Vehicle.h"
#include "Types.h"

TEST_CASE("Vehicle basic behavior and cost calculation", "[vehicle]") {
    Car car{42, "TestBrand", "ModelX", 4, 1000, VehicleStatus::Available};
    Vehicle v(car);

    REQUIRE(v.getId() == 42);
    REQUIRE(v.getBrand() == "TestBrand");
    REQUIRE(v.getModelOrType() == "ModelX");
    REQUIRE(v.getPricePerHour() == 1000);

    SECTION("Calculate cost for integer hours") {
        int cost = v.calculateRentalCost(3);
        REQUIRE(cost == 3000);
    }

    SECTION("Status setter/getter") {
        v.setStatus(VehicleStatus::Rented);
        REQUIRE(v.getStatus() == VehicleStatus::Rented);
        v.setStatus(VehicleStatus::Maintenance);
        REQUIRE(v.getStatus() == VehicleStatus::Maintenance);
    }

    SECTION("Negative price behaves predictably") {
        Car cheap = car;
        cheap.price_per_hour = -500;
        Vehicle neg(cheap);
        REQUIRE(neg.calculateRentalCost(2) == -1000);
    }
}

TEST_CASE("Vehicle Bike and Truck variants behavior", "[vehicle][variants]") {
    SECTION("Bike behavior and properties") {
        Bike bike{10, "Giant", "Road", 1250, VehicleStatus::Available};
        Vehicle v_bike(bike);

        REQUIRE(v_bike.getId() == 10);
        REQUIRE(v_bike.getBrand() == "Giant");
        REQUIRE(v_bike.getModelOrType() == "Road");
        REQUIRE(v_bike.getTypeString() == "Bike");
        REQUIRE(v_bike.getStatus() == VehicleStatus::Available);
        REQUIRE(v_bike.getStatusString() == "Available");
        REQUIRE(v_bike.getPricePerHour() == 1250);
        REQUIRE(v_bike.calculateRentalCost(4) == 5000);
    }

    SECTION("Truck behavior and properties") {
        Truck truck{20, "Scania", "R450", 18000, 4200, VehicleStatus::Maintenance};
        Vehicle v_truck(truck);

        REQUIRE(v_truck.getId() == 20);
        REQUIRE(v_truck.getBrand() == "Scania");
        REQUIRE(v_truck.getModelOrType() == "R450");
        REQUIRE(v_truck.getTypeString() == "Truck");
        REQUIRE(v_truck.getStatus() == VehicleStatus::Maintenance);
        REQUIRE(v_truck.getStatusString() == "Maintenance");
        REQUIRE(v_truck.getPricePerHour() == 4200);
        REQUIRE(v_truck.calculateRentalCost(5) == 21000);
    }
}
