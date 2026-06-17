#include "AppPaths.h"
#include "FleetMgmt.h"
#include "Logger.h"
#include "PrinterRegistry.h"
#include "StorageRegistry.h"
#include "UI.h"
#include "Vehicle.h"

#include <cstdlib>
#include <ctime>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <filesystem>
#include <memory>
#include <vector>

namespace {

auto loadRuntimePlugins(const std::filesystem::path& plugins_path) -> void {
    if (!std::filesystem::exists(plugins_path)) {
        return;
    }

#ifdef _WIN32
    const std::filesystem::path extension = ".dll";
#else
    const std::filesystem::path extension = ".so";
#endif

    for (const auto& entry : std::filesystem::directory_iterator(plugins_path)) {
        if (entry.path().extension() == extension) {
#ifdef _WIN32
            HMODULE handle = LoadLibraryW(entry.path().c_str());
            if (handle == nullptr) {
                continue;
            }
            using CreatorSignature = IPrinter* (*)(const char*);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            auto create_fn = reinterpret_cast<CreatorSignature>(GetProcAddress(handle, "create_printer"));
#pragma GCC diagnostic pop
#else
            void* handle = dlopen(entry.path().c_str(), RTLD_LAZY);
            if (handle == nullptr) {
                continue;
            }
            using CreatorSignature = IPrinter* (*)(const char*);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            auto create_fn = reinterpret_cast<CreatorSignature>(dlsym(handle, "create_printer"));
#endif

            if (create_fn != nullptr) {
                // Keep the library handle open
#ifdef _WIN32
                static std::vector<HMODULE> handles;
#else
                static std::vector<void*> handles;
#endif
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
#ifdef _WIN32
                FreeLibrary(handle);
#else
                dlclose(handle);
#endif
            }
        }
    }
}

} // namespace

auto main(int argc, char** argv) -> int {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Load plugins first so they are registered before help screen or configuration validation
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::filesystem::path exe_path(path);
    loadRuntimePlugins(exe_path.parent_path() / "plugins");
#else
    std::error_code err_code;
    auto exe_path = std::filesystem::read_symlink("/proc/self/exe", err_code);
    if (!err_code) {
        loadRuntimePlugins(exe_path.parent_path() / "plugins");
    }
#endif
    loadRuntimePlugins(std::filesystem::current_path() / "plugins");
    loadRuntimePlugins(std::filesystem::current_path() / "build/bin/plugins");
    loadRuntimePlugins(std::filesystem::current_path() / "build-win/bin/plugins");

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
        FleetManager init_mgr(std::move(init_storage));
        if (init_mgr.getFleet().empty()) {
            init_mgr.addVehicle(
                Vehicle(Car{101, "Tesla", "Model S", 5, 2500, VehicleStatus::Available}));
            init_mgr.addVehicle(
                Vehicle(Car{102, "Toyota", "Prius", 5, 1500, VehicleStatus::Available}));
            init_mgr.addVehicle(
                Vehicle(Bike{103, "Giant", "Escape", 500, VehicleStatus::Available}));
            init_mgr.addVehicle(
                Vehicle(Truck{104, "Volvo", "FH16", 25000, 5000, VehicleStatus::Available}));
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
