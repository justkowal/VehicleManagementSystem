#include <catch2/catch_test_macros.hpp>
#include "FileStorage.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("FileStorage basic save/load and malformed handling", "[filestorage]") {
    auto tmp = fs::temp_directory_path() / fs::path("vms_test_filestorage_") ;
    tmp += std::to_string(std::hash<std::string>{}(tmp.string()));
    fs::create_directories(tmp);

    // ensure clean
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    FileStorage storage(tmp.string());

    // empty fleet
    REQUIRE(storage.loadFleet().empty());

    // write/read fleet
    {
        Car car{7, "FBrand", "FModel", 2, 12.5, VehicleStatus::Available};
        Vehicle v(car);
        storage.saveFleet({v});
        auto loaded = storage.loadFleet();
        REQUIRE(loaded.size() == 1);
        REQUIRE(loaded[0].getId() == 7);
    }

    // malformed skipped
    {
        std::ofstream f((tmp / "fleet.txt").string(), std::ios::app);
        f << "This,is,not,a,valid,vehicle,line\n";
        f.close();
        auto loaded = storage.loadFleet();
        // still 1 vehicle
        REQUIRE(loaded.size() == 1);
    }

    // bad record file
    {
        std::ofstream rf((tmp / "records" / "7.txt").string(), std::ios::trunc);
        rf << "bad,line,without,proper,fields\n";
        rf.close();
        auto recs = storage.getRecordRange(7, 0, 10);
        REQUIRE(recs.empty());
    }

    fs::remove_all(tmp);
}

TEST_CASE("FileStorage directory creation failure throws", "[filestorage][failure]") {
    auto parent = fs::temp_directory_path() / fs::path("vms_test_parent_");
    parent += std::to_string(std::hash<std::string>{}(parent.string()));
    fs::remove_all(parent);
    fs::create_directories(parent);

    // make parent non-writable
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
