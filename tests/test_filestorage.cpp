#include <catch2/catch_test_macros.hpp>
#include "FileStorage.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("FileStorage basic save/load and malformed handling", "[filestorage]") {
    auto tmp = fs::temp_directory_path() / fs::path("vms_test_filestorage_") ;
    tmp += std::to_string(std::hash<std::string>{}(tmp.string()));
    fs::create_directories(tmp);

    fs::remove_all(tmp);
    fs::create_directories(tmp);

    FileStorage storage(tmp.string());

    REQUIRE(storage.loadFleet().empty());

    {
        Car car{7, "FBrand", "FModel", 2, 1250, VehicleStatus::Available};
        Vehicle v(car);
        storage.saveFleet({v});
        auto loaded = storage.loadFleet();
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded[0].getId() == 7);
    }

    {
        std::ofstream f((tmp / "fleet.txt").string(), std::ios::app);
        f << "This,is,not,a,valid,vehicle,line\n";
        f.close();
        auto loaded = storage.loadFleet();
        REQUIRE(loaded.size() == 1);
    }

    {
        std::ofstream rf((tmp / "records" / "7.txt").string(), std::ios::trunc);
        rf << "bad,line,without,proper,fields\n";
        rf.close();
        auto recs = storage.getRecordRange(7, 0, 10);
        REQUIRE(recs.empty());
    }

    fs::remove_all(tmp);
}

#ifndef _WIN32
TEST_CASE("FileStorage directory creation failure throws", "[filestorage][failure]") {
    auto parent = fs::temp_directory_path() / fs::path("vms_test_parent_");
    parent += std::to_string(std::hash<std::string>{}(parent.string()));
    fs::remove_all(parent);
    fs::create_directories(parent);

    // make parent unwritable
    fs::permissions(parent, fs::perms::owner_read | fs::perms::owner_exec);

    auto bad_path = parent / "child_unwritable";

    bool threw = false;
    try {
        FileStorage s(bad_path.string());
    } catch (const std::exception&) {
        threw = true;
    }

    // restore perms
    fs::permissions(parent, fs::perms::owner_all);
    fs::remove_all(parent);

    REQUIRE(threw == true);
}
#endif

TEST_CASE("FileStorage serialization and deserialization of Bike and Truck variants", "[filestorage]") {
    auto tmp_dir = fs::temp_directory_path() / fs::path("vms_test_variants_");
    tmp_dir += std::to_string(std::hash<std::string>{}(tmp_dir.string()));
    fs::create_directories(tmp_dir);

    FileStorage storage(tmp_dir.string());

    Bike bike{12, "Trek", "Hybrid", 1000, VehicleStatus::Available};
    Truck truck{22, "Volvo", "FH16", 24000, 6000, VehicleStatus::Rented};

    std::vector<Vehicle> fleet = {Vehicle(bike), Vehicle(truck)};
    storage.saveFleet(fleet);

    auto loaded = storage.loadFleet();
    REQUIRE(loaded.size() == 2);

    // check bike loading
    REQUIRE(loaded[0].getId() == 12);
    REQUIRE(loaded[0].getBrand() == "Trek");
    REQUIRE(loaded[0].getModelOrType() == "Hybrid");
    REQUIRE(loaded[0].getTypeString() == "Bike");
    REQUIRE(loaded[0].getPricePerHour() == 1000);
    REQUIRE(loaded[0].getStatus() == VehicleStatus::Available);

    // check truck loading
    REQUIRE(loaded[1].getId() == 22);
    REQUIRE(loaded[1].getBrand() == "Volvo");
    REQUIRE(loaded[1].getModelOrType() == "FH16");
    REQUIRE(loaded[1].getTypeString() == "Truck");
    REQUIRE(loaded[1].getPricePerHour() == 6000);
    REQUIRE(loaded[1].getStatus() == VehicleStatus::Rented);

    std::visit([](auto&& obj) {
        using T = std::decay_t<decltype(obj)>;
        if constexpr (std::is_same_v<T, Truck>) {
            REQUIRE(obj.payload_capacity_kg == 24000);
        } else {
            FAIL("Expected second vehicle to be a Truck variant");
        }
    }, loaded[1].getVariant());

    fs::remove_all(tmp_dir);
}

TEST_CASE("FileStorage active rentals parsing robustness and error handling", "[filestorage][robustness]") {
    auto tmp_dir = fs::temp_directory_path() / fs::path("vms_test_robustness_");
    tmp_dir += std::to_string(std::hash<std::string>{}(tmp_dir.string()));
    fs::create_directories(tmp_dir);

    // write malformed rentals.txt
    {
        std::ofstream f(tmp_dir / "rentals.txt", std::ios::trunc);
        f << "RENT-MALFORMED,non_numeric_id,2026-06-11T19:00:00Z\n"
          << "RENT-VALID,305,2026-06-11T19:30:00Z\n"
          << "RENT-SHORT,101\n"; // too few parameters
    }

    FileStorage storage(tmp_dir.string());
    
    // call loadactiverentals
    std::unordered_map<std::string, RentalSession> active_rentals;
    REQUIRE_NOTHROW(active_rentals = storage.loadActiveRentals());

    // verify only valid session returned
    REQUIRE(active_rentals.size() == 1);
    REQUIRE(active_rentals.contains("RENT-VALID") == true);
    REQUIRE(active_rentals["RENT-VALID"].vehicle_id == 305);

    fs::remove_all(tmp_dir);
}
