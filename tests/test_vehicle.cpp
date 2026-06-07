#include <catch2/catch_test_macros.hpp>
#include "Vehicle.h"
#include "Types.h"

TEST_CASE("Vehicle basic behavior and cost calculation", "[vehicle]") {
    Car car{42, "TestBrand", "ModelX", 4, 10.0, VehicleStatus::Available};
    Vehicle v(car);

    REQUIRE(v.getId() == 42);
    REQUIRE(v.getBrand() == "TestBrand");
    REQUIRE(v.getModelOrType() == "ModelX");
    REQUIRE(v.getPricePerHour() == 10.0);

    SECTION("Calculate cost for integer hours") {
        double cost = v.calculateRentalCost(3);
        REQUIRE(cost == 30.0);
    }

    SECTION("Status setter/getter") {
        v.setStatus(VehicleStatus::Rented);
        REQUIRE(v.getStatus() == VehicleStatus::Rented);
        v.setStatus(VehicleStatus::Maintenance);
        REQUIRE(v.getStatus() == VehicleStatus::Maintenance);
    }

    SECTION("Negative price behaves predictably") {
        Car cheap = car;
        cheap.price_per_hour = -5.0;
        Vehicle neg(cheap);
        REQUIRE(neg.calculateRentalCost(2) == -10.0);
    }
}
