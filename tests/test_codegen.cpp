#include <catch2/catch_test_macros.hpp>
#include "FleetMgmt.h"
#include "MockSystem.h"
#include <unordered_set>

TEST_CASE("Rental code generation uniqueness (basic)", "[codegen][stress]") {
    auto storage = std::make_unique<MockStorage>();
    auto printer = std::make_unique<MockPrinter>();

    FleetManager manager(std::move(storage), std::move(printer));

    const char* env = std::getenv("VMS_STRESS_ITERS");
    int N = 1000;
    if (env) {
        try { N = std::stoi(env); } catch(...) { N = 1000; }
    }
    for (int i = 0; i < N; ++i) {
        Car car{static_cast<uint32_t>(1000 + i), "Brand", "Model", 4, 100 + (i % 10) * 100, VehicleStatus::Available};
        manager.addVehicle(Vehicle(car));
    }

    std::unordered_set<std::string> codes;
    codes.reserve(N);

    for (int i = 0; i < N; ++i) {
        auto code = manager.rentVehicle(1000 + i, "John", "Doe", "AB123456");
        REQUIRE(code.has_value());
        auto inserted = codes.insert(code.value()).second;
        REQUIRE(inserted == true);
    }
}
