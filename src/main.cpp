#include "AppPaths.h"
#include "FleetMgmt.h"
#include "PrinterRegistry.h"
#include "StorageRegistry.h"
#include "UI.h"
#include "Vehicle.h"
#include "Logger.h"

#include <cstdlib>
#include <ctime>
#include <memory>
#include <iostream>

auto main(int argc, char** argv) -> int {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // handle help first
    if (AppPaths::handleHelp(argc, argv)) {
        return EXIT_SUCCESS;
    }

    // resolve and validate config
    if (!AppPaths::resolve(argc, argv)) {
        return EXIT_FAILURE;
    }

    Logger::getInstance().setLogFile((AppPaths::dataDir() / "vms.log").string());

    const std::string data_path    = AppPaths::dataDir().string();
    const std::string storage_name = AppPaths::storageName();
    const std::string printer_name = AppPaths::printerName();
    const std::string printer_dev  = AppPaths::printerDevice();

    // populate default fleet on first launch
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
        LOG_FATAL("System crash: " + std::string(e.what()));
        return EXIT_FAILURE;
    } catch (...) {
        LOG_FATAL("Unknown system crash occurred.");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
