#include "AppPaths.h"
#include "FleetMgmt.h"
#include "Logger.h"
#include "PrinterRegistry.h"
#include "StorageRegistry.h"
#include "UI.h"
#include "Vehicle.h"

#include <cstdlib>
#include <ctime>
#include <dlfcn.h>
#include <filesystem>
#include <memory>
#include <vector>

namespace {

auto loadRuntimePlugins(const std::filesystem::path& plugins_path) -> void {
    if (!std::filesystem::exists(plugins_path)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(plugins_path)) {
        if (entry.path().extension() == ".so") {
            void* handle = dlopen(entry.path().c_str(), RTLD_LAZY);
            if (handle == nullptr) {
                continue;
            }

            using CreatorSignature = auto (*)(const char*)->IPrinter*;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            auto create_fn = reinterpret_cast<CreatorSignature>(dlsym(handle, "create_printer"));

            if (create_fn != nullptr) {
                // Keep the library handle open
                static std::vector<void*> handles;
                handles.push_back(handle);

                std::string name = entry.path().stem().string();
                if (name.starts_with("lib")) {
                    name = name.substr(3);
                }
                if (name.ends_with("_plugin")) {
                    name = name.substr(0, name.size() - 7);
                }

                PrinterRegistry::add(
                    name,
                    [create_fn](const std::string& config) -> std::unique_ptr<IPrinter> {
                        return std::unique_ptr<IPrinter>(create_fn(config.c_str()));
                    },
                    nullptr, false);
            } else {
                dlclose(handle);
            }
        }
    }
}

} // namespace

auto main(int argc, char** argv) -> int {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Load plugins first so they are registered before help screen or configuration validation
    std::error_code err_code;
    auto exe_path = std::filesystem::read_symlink("/proc/self/exe", err_code);
    if (!err_code) {
        loadRuntimePlugins(exe_path.parent_path() / "plugins");
    }
    loadRuntimePlugins(std::filesystem::current_path() / "plugins");
    loadRuntimePlugins(std::filesystem::current_path() / "build/bin/plugins");

    // handle help first
    if (AppPaths::handleHelp(argc, argv)) {
        return EXIT_SUCCESS;
    }

    // resolve and validate config
    if (!AppPaths::resolve(argc, argv)) {
        return EXIT_FAILURE;
    }

    Logger::getInstance().setLogFile((AppPaths::dataDir() / "vms.log").string());

    const std::string data_path = AppPaths::dataDir().string();
    const std::string storage_name = AppPaths::storageName();
    const std::string printer_name = AppPaths::printerName();
    const std::string printer_dev = AppPaths::printerDevice();

    // populate default fleet on first launch
    {
        auto init_storage = StorageRegistry::create(storage_name, data_path);
        auto current_fleet = init_storage->loadFleet();
        if (current_fleet.empty()) {
            if (AppPaths::massiveInit()) {
                std::vector<Vehicle> massive_fleet;
                massive_fleet.reserve(10000);
                for (uint32_t i = 1; i <= 10000; ++i) {
                    uint32_t type_selector = i % 3;
                    if (type_selector == 0) {
                        Car car{};
                        car.id = i;
                        car.brand = "BrandCar" + std::to_string(i);
                        car.model = "ModelCar" + std::to_string(i);
                        car.seats = static_cast<uint8_t>(2 + (i % 7));
                        car.price_per_hour = static_cast<int>(1000 + (i % 9000));
                        car.status = VehicleStatus::Available;
                        massive_fleet.emplace_back(car);
                    } else if (type_selector == 1) {
                        Bike bike{};
                        bike.id = i;
                        bike.brand = "BrandBike" + std::to_string(i);
                        bike.type = "TypeBike" + std::to_string(i);
                        bike.price_per_hour = static_cast<int>(200 + (i % 800));
                        bike.status = VehicleStatus::Available;
                        massive_fleet.emplace_back(bike);
                    } else {
                        Truck truck{};
                        truck.id = i;
                        truck.brand = "BrandTruck" + std::to_string(i);
                        truck.model = "ModelTruck" + std::to_string(i);
                        truck.payload_capacity_kg = 1000 + (i % 20000);
                        truck.price_per_hour = static_cast<int>(3000 + (i % 17000));
                        truck.status = VehicleStatus::Available;
                        massive_fleet.emplace_back(truck);
                    }
                }
                init_storage->saveFleet(massive_fleet);
            } else {
                std::vector<Vehicle> default_fleet;
                default_fleet.emplace_back(Car{101, "Tesla", "Model S", 5, 2500, VehicleStatus::Available});
                default_fleet.emplace_back(Car{102, "Toyota", "Prius", 5, 1500, VehicleStatus::Available});
                default_fleet.emplace_back(Bike{103, "Giant", "Escape", 500, VehicleStatus::Available});
                default_fleet.emplace_back(Truck{104, "Volvo", "FH16", 25000, 5000, VehicleStatus::Available});
                init_storage->saveFleet(default_fleet);
            }
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
