#include "AppPaths.h"
#include "FleetMgmt.h"
#include "PrinterRegistry.h"
#include "StorageRegistry.h"
#include "UI.h"
#include "Vehicle.h"

#include <cstdlib>
#include <ctime>
#include <memory>
#include <iostream>

auto main(int argc, char** argv) -> int {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Handle --help first (reads from registries, no side effects)
    if (AppPaths::handleHelp(argc, argv)) {
        return EXIT_SUCCESS;
    }

    // Resolve and validate all config (data path, storage backend, printer).
    // On failure, AppPaths prints a diagnostic and returns false.
    if (!AppPaths::resolve(argc, argv)) {
        return EXIT_FAILURE;
    }

    const std::string data_path    = AppPaths::dataDir().string();
    const std::string storage_name = AppPaths::storageName();
    const std::string printer_name = AppPaths::printerName();
    const std::string printer_dev  = AppPaths::printerDevice();

    // Pre-populate the fleet database with showcase vehicles on first launch.
    {
        auto init_storage = StorageRegistry::create(storage_name, data_path);
        FleetManager init_mgr(std::move(init_storage));
        if (init_mgr.getFleet().empty()) {
            init_mgr.addVehicle(Vehicle(Car{101, "Tesla", "Model S", 5, 2500, VehicleStatus::Available}));
            init_mgr.addVehicle(Vehicle(Car{102, "Toyota", "Prius", 5, 1500, VehicleStatus::Available}));
            init_mgr.addVehicle(Vehicle(Bike{103, "Giant", "Escape", 500, VehicleStatus::Available}));
            init_mgr.addVehicle(Vehicle(Truck{104, "Volvo", "FH16", 25000, 5000, VehicleStatus::Available}));
        }
    }

    try {
        auto storage = StorageRegistry::create(storage_name, data_path);
        auto printer = PrinterRegistry::create(printer_name, printer_dev);

        FleetManager manager(std::move(storage), std::move(printer));

        UI app(manager);
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "\n[Fatal Error] System crash: " << e.what() << "\n";
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "\n[Fatal Error] Unknown system crash occurred.\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
