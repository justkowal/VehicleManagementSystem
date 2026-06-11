#include "AppPaths.h"
#include "PrinterRegistry.h"
#include "StorageRegistry.h"

#include <cstdlib>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
fs::path    AppPaths::data_dir_;
std::string AppPaths::storage_name_;
std::string AppPaths::printer_name_;
std::string AppPaths::printer_device_;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

constexpr std::string_view DEFAULT_PRINTER_DEVICE = "127.0.0.1:9100";

auto osError(const std::error_code& err) -> std::string {
    return err.message();
}

// Joins a vector of strings with a separator for display.
auto join(const std::vector<std::string>& vec, std::string_view sep) -> std::string {
    if (vec.empty()) {
        return "(none)";
    }
    std::string result = vec.front();
    for (size_t idx = 1; idx < vec.size(); ++idx) {
        result += sep;
        result += vec[idx];
    }
    return result;
}

// Trims leading and trailing whitespace.
auto trim(std::string_view str) -> std::string {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(first, (last - first + 1)));
}

} // namespace

// ---------------------------------------------------------------------------
// Platform default data directory
// ---------------------------------------------------------------------------

auto AppPaths::platformDefaultDataDir() -> fs::path {
#if defined(NDEBUG) // Release build — use OS user-data location

#if defined(_WIN32)
    // NOLINTNEXTLINE(concurrency-mt-unsafe) — called once at startup before any threads
    const char* appdata = std::getenv("APPDATA");
    if (appdata != nullptr) {
        return fs::path(appdata) / "VehicleRentalSystem";
    }
    return fs::current_path() / "data";

#else
    // NOLINTNEXTLINE(concurrency-mt-unsafe) — called once at startup before any threads
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return fs::path(xdg) / "VehicleRentalSystem";
    }

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return fs::path(home) / ".local" / "share" / "VehicleRentalSystem";
    }

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const passwd* pw = getpwuid(getuid());
    if (pw != nullptr) {
        return fs::path(pw->pw_dir) / ".local" / "share" / "VehicleRentalSystem";
    }

    return fs::current_path() / "data";
#endif

#else  // Debug build — keep data next to the working directory
    return {"data"};
#endif
}

// ---------------------------------------------------------------------------
// Path validation
// ---------------------------------------------------------------------------

auto AppPaths::validateAndCreate(const fs::path& path) -> bool {
    std::error_code err;

    fs::create_directories(path / "records", err);
    if (err) {
        std::cerr << "[Error] Data directory could not be created: " << path << "\n"
                  << "        Reason: " << osError(err) << "\n"
                  << "        Suggestion: Use --data-dir <path> to specify a writable location.\n";
        return false;
    }

    const fs::path probe = path / ".write_probe";
    {
        std::ofstream test_file(probe);
        if (!test_file.is_open()) {
            std::cerr << "[Error] Data directory is not writable: " << path << "\n"
                      << "        Suggestion: Use --data-dir <path> to specify a writable location.\n";
            return false;
        }
    }

    fs::remove(probe, err); // best-effort cleanup
    return true;
}

// ---------------------------------------------------------------------------
// Help text
// ---------------------------------------------------------------------------

auto AppPaths::printHelp(const char* prog_name, std::ostream& out_stream) -> void {
    const auto storage_list = StorageRegistry::available();
    const auto printer_list = PrinterRegistry::available();

    out_stream  << "Usage: " << prog_name << " [OPTIONS]\n" // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        << "\nVehicle Rental & Fleet Management System\n"
        << "\nData directory:\n"
        << "  --data-dir <path>         Use a custom data directory path\n"
        << "  --local                   Use ./data (current working directory)\n"
        << "\nStorage backend:\n"
        << "  --storage <name>          Implementation to use (default: "
        <<                               StorageRegistry::defaultName() << ")\n"
        << "                            Available: " << join(storage_list, ", ") << "\n"
        << "\nPrinter backend:\n"
        << "  --printer <name>          Implementation to use (default: "
        <<                               PrinterRegistry::defaultName() << ")\n"
        << "                            Available: " << join(printer_list, ", ") << "\n"
        << "  --printer-device <addr>   Device address / path (default: "
        <<                               DEFAULT_PRINTER_DEVICE << ")\n"
        << "\nOther:\n"
        << "  --help, -h                Show this message\n"
        << "\nData directory (default):\n"
#if defined(NDEBUG)
#if defined(_WIN32)
        << "  Release build:  %APPDATA%\\VehicleRentalSystem\n"
#else
        << "  Release build:  $XDG_DATA_HOME/VehicleRentalSystem\n"
        << "                  (~/.local/share/VehicleRentalSystem if XDG_DATA_HOME unset)\n"
#endif
#else
        << "  Debug build:    ./data\n"
#endif
        << "\n";
}

auto AppPaths::handleHelp(int argc, char** argv) -> bool {
    for (int idx = 1; idx < argc; ++idx) {
        const std::string_view arg(argv[idx]); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (arg == "--help" || arg == "-h") {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            printHelp(argv[0]);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Config file loading
// ---------------------------------------------------------------------------

auto AppPaths::loadConfigFile(ConfigArgs& out_args) -> bool {
    const fs::path config_path = fs::current_path() / "vrs.conf";
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return true; // Missing file is not an error
    }

    std::string line;
    size_t line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        std::string_view line_view(line);

        // Trim leading space
        size_t first = line_view.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            continue; // Empty line
        }
        line_view = line_view.substr(first);
        if (line_view.starts_with('#')) {
            continue; // Comment line
        }

        size_t eq_pos = line_view.find('=');
        if (eq_pos == std::string_view::npos) {
            std::cerr << "[Error] Malformed line in vrs.conf:" << line_num << ": \"" << line << "\"\n"
                      << "        Expected 'key=value'.\n";
            return false;
        }

        std::string key = trim(line_view.substr(0, eq_pos));
        std::string val = trim(line_view.substr(eq_pos + 1));

        if (key == "storage") {
            out_args.storage_arg = val;
        } else if (key == "printer") {
            out_args.printer_arg = val;
        } else if (key == "printer-device" || key == "printer_device") {
            out_args.device_arg = val;
        } else if (key == "data-dir" || key == "data_dir") {
            out_args.data_override = fs::path(val);
        } else if (key == "local") {
            if (val == "true" || val == "1") {
                out_args.data_override = fs::path("data");
            }
        } else {
            std::cerr << "[Error] Unknown configuration key in vrs.conf:" << line_num << ": \"" << key << "\"\n";
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// CLI parsing and resolution
// ---------------------------------------------------------------------------

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto AppPaths::parseSingleArg(std::string_view arg,
                              int& idx,
                              int argc,
                              char** argv,
                              ConfigArgs& out_args) -> bool {
    // ---- Data directory flags --------------------------------------------
    if (arg == "--local") {
        out_args.data_override = fs::path("data");
        return true;
    }
    if (arg == "--data-dir") {
        if (idx + 1 >= argc) {
            std::cerr << "[Error] --data-dir requires a path argument.\n"
                      << "        Example: --data-dir /home/user/fleet-data\n";
            return false;
        }
        ++idx;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out_args.data_override = fs::path(argv[idx]);
        return true;
    }
    if (arg.starts_with("--data-dir=")) {
        out_args.data_override = fs::path(arg.substr(11));
        return true;
    }

    // ---- Storage backend flag ---------------------------------------------
    if (arg == "--storage") {
        if (idx + 1 >= argc) {
            std::cerr << "[Error] --storage requires a backend name.\n"
                      << "        Available: " << join(StorageRegistry::available(), ", ") << "\n";
            return false;
        }
        ++idx;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out_args.storage_arg = argv[idx];
        return true;
    }
    if (arg.starts_with("--storage=")) {
        out_args.storage_arg = std::string(arg.substr(10));
        return true;
    }

    // ---- Printer backend flags --------------------------------------------
    if (arg == "--printer") {
        if (idx + 1 >= argc) {
            std::cerr << "[Error] --printer requires a backend name.\n"
                      << "        Available: " << join(PrinterRegistry::available(), ", ") << "\n";
            return false;
        }
        ++idx;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out_args.printer_arg = argv[idx];
        return true;
    }
    if (arg.starts_with("--printer=")) {
        out_args.printer_arg = std::string(arg.substr(10));
        return true;
    }
    if (arg == "--printer-device") {
        if (idx + 1 >= argc) {
            std::cerr << "[Error] --printer-device requires an address argument.\n"
                      << "        Example: --printer-device 192.168.1.50:9100\n";
            return false;
        }
        ++idx;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out_args.device_arg = argv[idx];
        return true;
    }
    if (arg.starts_with("--printer-device=")) {
        out_args.device_arg = std::string(arg.substr(17));
        return true;
    }

    // ---- Help flags (ignored here, resolved in handleHelp) ---------------
    if (arg == "--help" || arg == "-h") {
        return true;
    }

    // ---- Unknown flag -----------------------------------------------------
    if (arg.starts_with("-")) {
        std::cerr << "[Error] Unknown flag: \"" << arg << "\"\n\n";
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        printHelp(argv[0], std::cerr);
        return false;
    }

    return true;
}

auto AppPaths::parseCommandLine(int argc,
                                char** argv,
                                ConfigArgs& out_args) -> bool {
    for (int idx = 1; idx < argc; ++idx) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (!parseSingleArg(argv[idx], idx, argc, argv, out_args)) {
            return false;
        }
    }
    return true;
}

auto AppPaths::resolve(int argc, char** argv) -> bool {
    ConfigArgs args{};

    // ---- 1. Load config file values first (CWD relative) -----------------
    if (!loadConfigFile(args)) {
        return false;
    }

    // ---- 2. Parse CLI flags (override config values) ---------------------
    if (!parseCommandLine(argc, argv, args)) {
        return false;
    }

    // ---- 3. Validate storage selection -----------------------------------
    const std::string resolved_storage =
        args.storage_arg.empty() ? StorageRegistry::defaultName() : args.storage_arg;

    if (!StorageRegistry::contains(resolved_storage)) {
        std::cerr << "[Error] Unknown storage backend: \"" << resolved_storage << "\"\n"
                  << "        Available: " << join(StorageRegistry::available(), ", ") << "\n\n";
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        printHelp(argv[0], std::cerr);
        return false;
    }

    // ---- 4. Validate printer selection -----------------------------------
    const std::string resolved_printer =
        args.printer_arg.empty() ? PrinterRegistry::defaultName() : args.printer_arg;

    if (!PrinterRegistry::contains(resolved_printer)) {
        std::cerr << "[Error] Unknown printer backend: \"" << resolved_printer << "\"\n"
                  << "        Available: " << join(PrinterRegistry::available(), ", ") << "\n\n";
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        printHelp(argv[0], std::cerr);
        return false;
    }

    // ---- 5. Resolve candidate values -------------------------------------
    const fs::path candidate = args.data_override.value_or(platformDefaultDataDir());
    const std::string resolved_device = args.device_arg.empty() ? std::string(DEFAULT_PRINTER_DEVICE) : args.device_arg;

    // ---- 6. Run custom validation lambdas --------------------------------
    if (auto err_msg = StorageRegistry::validate(resolved_storage, candidate.string())) {
        std::cerr << "[Error] " << *err_msg << "\n";
        return false;
    }

    if (auto err_msg = PrinterRegistry::validate(resolved_printer, resolved_device)) {
        std::cerr << "[Error] " << *err_msg << "\n";
        return false;
    }

    // ---- 7. Validate directory writability & creation --------------------
    if (!validateAndCreate(candidate)) {
        return false;
    }

    // ---- 8. Commit all resolved config -----------------------------------
    data_dir_       = candidate;
    storage_name_   = resolved_storage;
    printer_name_   = resolved_printer;
    printer_device_ = resolved_device;

    return true;
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

auto AppPaths::dataDir()       -> const fs::path&   { return data_dir_; }
auto AppPaths::storageName()   -> const std::string& { return storage_name_; }
auto AppPaths::printerName()   -> const std::string& { return printer_name_; }
auto AppPaths::printerDevice() -> const std::string& { return printer_device_; }
