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

struct CarBrandModels {
    std::string brand;
    std::vector<std::string> models;
};

struct BikeBrandTypes {
    std::string brand;
    std::vector<std::string> types;
};

struct TruckBrandModels {
    std::string brand;
    std::vector<std::string> models;
};

const std::vector<CarBrandModels> CAR_BRANDS = {
    {"Toyota", {"Corolla", "Camry", "RAV4", "Prius", "Yaris", "Land Cruiser", "Highlander", "Supra", "Sienna", "Tacoma"}},
    {"Honda", {"Civic", "Accord", "CR-V", "Pilot", "Fit", "Odyssey", "HR-V", "Insight", "Passport", "Ridgeline"}},
    {"Ford", {"Mustang", "Focus", "Explorer", "F-150", "Escape", "Bronco", "Edge", "Ranger", "Fusion", "Expedition"}},
    {"BMW", {"3 Series", "5 Series", "X5", "i4", "iX", "7 Series", "M3", "X3", "2 Series", "8 Series"}},
    {"Tesla", {"Model 3", "Model Y", "Model S", "Model X", "Cybertruck", "Roadster"}},
    {"Chevrolet", {"Corvette", "Camaro", "Malibu", "Bolt EV", "Tahoe", "Equinox", "Suburban", "Silverado", "Colorado", "Trailblazer"}},
    {"Mercedes-Benz", {"C-Class", "E-Class", "S-Class", "GLC", "GLE", "AMG GT", "EQS", "A-Class", "CLA", "GLA"}},
    {"Audi", {"A4", "A6", "Q5", "Q7", "e-tron", "R8", "A3", "A5", "Q3", "TT"}},
    {"Volkswagen", {"Golf", "Passat", "Tiguan", "ID.4", "Jetta", "Touareg", "Polo", "Arteon", "Taos", "Atlas"}},
    {"Porsche", {"911", "Cayenne", "Macan", "Taycan", "Panamera", "718 Cayman", "Boxster"}},
    {"Hyundai", {"Elantra", "Sonata", "Tucson", "Santa Fe", "Ioniq 5", "Kona", "Palisade", "Accent", "Veloster", "Venue"}},
    {"Nissan", {"Sentra", "Altima", "Rogue", "Leaf", "GT-R", "Pathfinder", "Versa", "Murano", "Maxima", "Frontier"}},
    {"Mazda", {"Mazda3", "Mazda6", "CX-5", "CX-30", "CX-9", "MX-5 Miata", "CX-50"}},
    {"Subaru", {"Impreza", "Legacy", "Outback", "Forester", "Crosstrek", "WRX", "BRZ", "Ascent"}},
    {"Lexus", {"IS", "ES", "RX", "NX", "LC", "LS", "GX", "UX"}}
};

const std::vector<BikeBrandTypes> BIKE_BRANDS = {
    {"Giant", {"Road", "Mountain", "Hybrid", "Electric", "Gravel"}},
    {"Trek", {"Road", "Mountain", "Hybrid", "Electric", "Gravel", "City"}},
    {"Specialized", {"Road", "Mountain", "Active", "Electric", "Gravel"}},
    {"Cannondale", {"Road", "Mountain", "Active", "Electric", "Gravel"}},
    {"Bianchi", {"Road", "Gravel", "Electric", "City"}},
    {"Scott", {"Road", "Mountain", "Gravel", "Electric"}},
    {"Santa Cruz", {"Mountain", "Gravel", "Electric"}},
    {"Canyon", {"Road", "Mountain", "Gravel", "Electric", "City"}},
    {"Pinarello", {"Road", "Gravel", "Electric"}},
    {"Cervelo", {"Road", "Gravel", "Triathlon"}},
    {"Marin", {"Mountain", "Gravel", "Transit", "Hardtail"}},
    {"Kona", {"Mountain", "Gravel", "Electric", "Commuter"}},
    {"Raleigh", {"Classic", "City", "Hybrid", "Electric"}}
};

const std::vector<TruckBrandModels> TRUCK_BRANDS = {
    {"Volvo", {"FH16", "FMX", "FH", "FM", "FE", "FL", "VNL", "VHD", "VAH"}},
    {"Scania", {"R-Series", "S-Series", "G-Series", "P-Series", "L-Series", "XT-Range"}},
    {"Mercedes-Benz", {"Actros", "Arocs", "Atego", "Econic", "Unimog", "Zetros"}},
    {"Ford", {"F-150", "F-250", "F-350", "F-450", "F-550", "F-650", "F-750", "Transit", "Ranger"}},
    {"Isuzu", {"N-Series", "F-Series", "C-Series", "E-Series", "D-Max"}},
    {"Freightliner", {"Cascadia", "M2 106", "M2 112", "108SD", "114SD", "122SD"}},
    {"Kenworth", {"T680", "T880", "W990", "T280", "T380", "T480", "K270", "K370"}},
    {"Peterbilt", {"Model 579", "Model 389", "Model 567", "Model 520", "Model 220", "Model 548", "Model 536"}},
    {"DAF", {"XF", "XG+", "XG", "XD", "LF", "CF"}},
    {"MAN", {"TGX", "TGS", "TGM", "TGL"}},
    {"Iveco", {"S-Way", "T-Way", "X-Way", "Eurocargo", "Daily"}},
    {"Chevrolet", {"Silverado 1500", "Silverado 2500HD", "Silverado 3500HD", "Colorado", "Express Cargo"}},
    {"GMC", {"Sierra 1500", "Sierra 2500HD", "Sierra 3500HD", "Canyon", "Savana"}},
    {"RAM", {"1500", "2500", "3500", "Chassis Cab", "ProMaster"}}
};

struct BrandModelResult {
    std::string brand;
    std::string model_or_type;
};

auto getCarBrandAndModel(uint32_t index) -> BrandModelResult {
    if (CAR_BRANDS.empty()) {
        return {};
    }
    size_t brand_idx = index % CAR_BRANDS.size();
    const auto& brand_entry = CAR_BRANDS[brand_idx];
    std::string model;
    if (!brand_entry.models.empty()) {
        size_t model_idx = (index / CAR_BRANDS.size()) % brand_entry.models.size();
        model = brand_entry.models[model_idx];
    }
    return {brand_entry.brand, model};
}

auto getBikeBrandAndType(uint32_t index) -> BrandModelResult {
    if (BIKE_BRANDS.empty()) {
        return {};
    }
    size_t brand_idx = index % BIKE_BRANDS.size();
    const auto& brand_entry = BIKE_BRANDS[brand_idx];
    std::string type;
    if (!brand_entry.types.empty()) {
        size_t type_idx = (index / BIKE_BRANDS.size()) % brand_entry.types.size();
        type = brand_entry.types[type_idx];
    }
    return {brand_entry.brand, type};
}

auto getTruckBrandAndModel(uint32_t index) -> BrandModelResult {
    if (TRUCK_BRANDS.empty()) {
        return {};
    }
    size_t brand_idx = index % TRUCK_BRANDS.size();
    const auto& brand_entry = TRUCK_BRANDS[brand_idx];
    std::string model;
    if (!brand_entry.models.empty()) {
        size_t model_idx = (index / TRUCK_BRANDS.size()) % brand_entry.models.size();
        model = brand_entry.models[model_idx];
    }
    return {brand_entry.brand, model};
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
                        auto result = getCarBrandAndModel(i);
                        car.brand = std::move(result.brand);
                        car.model = std::move(result.model_or_type);
                        car.seats = static_cast<uint8_t>(2 + (i % 7));
                        car.price_per_hour = static_cast<int>(1000 + (i % 9000));
                        car.status = VehicleStatus::Available;
                        massive_fleet.emplace_back(car);
                    } else if (type_selector == 1) {
                        Bike bike{};
                        bike.id = i;
                        auto result = getBikeBrandAndType(i);
                        bike.brand = std::move(result.brand);
                        bike.type = std::move(result.model_or_type);
                        bike.price_per_hour = static_cast<int>(200 + (i % 800));
                        bike.status = VehicleStatus::Available;
                        massive_fleet.emplace_back(bike);
                    } else {
                        Truck truck{};
                        truck.id = i;
                        auto result = getTruckBrandAndModel(i);
                        truck.brand = std::move(result.brand);
                        truck.model = std::move(result.model_or_type);
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
