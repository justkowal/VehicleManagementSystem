#include "AppPaths.h"
#include "PrinterRegistry.h"
#include "StorageRegistry.h"
#include "Logger.h"

#include <cstdlib>
#include <fstream>
#include <system_error>
#include <regex>

#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
fs::path    AppPaths::data_dir_;
std::string AppPaths::storage_name_;
std::string AppPaths::printer_name_;
std::string AppPaths::printer_device_;
bool        AppPaths::massive_init_ = false;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

namespace {

constexpr std::string_view DEFAULT_PRINTER_DEVICE = "127.0.0.1:9100";

auto osError(const std::error_code& err) -> std::string {
    return err.message();
}

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

auto trim(std::string_view str) -> std::string {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(first, (last - first + 1)));
}

} 

auto AppPaths::platformDefaultDataDir() -> fs::path {
#if defined(NDEBUG) 
    // NOLINTNEXTLINE(concurrency-mt-unsafe) — called once at startup before any threads
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg != nullptr && xdg[0] != '\0') {
        return fs::path(xdg) / "VehicleManagementSystem";
    }

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* home = std::getenv("HOME");
    if (home != nullptr) {
        return fs::path(home) / ".local" / "share" / "VehicleManagementSystem";
    }

    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const passwd* passwd_ptr = getpwuid(getuid());
    if (passwd_ptr != nullptr) {
        return fs::path(passwd_ptr->pw_dir) / ".local" / "share" / "VehicleManagementSystem";
    }

    return fs::current_path() / "data";
#else  
    return {"data"};
#endif
}

auto AppPaths::validateAndCreate(const fs::path& path) -> bool {
    std::error_code err;

    fs::create_directories(path / "records", err);
    if (err) {
        LOG_ERROR("Data directory could not be created: " + path.string() + "\n"
                  "        Reason: " + osError(err) + "\n"
                  "        Suggestion: Use --data-dir <path> to specify a writable location.");
        return false;
    }

    const fs::path probe = path / ".write_probe";
    {
        std::ofstream test_file(probe);
        if (!test_file.is_open()) {
            LOG_ERROR("Data directory is not writable: " + path.string() + "\n"
                      "        Suggestion: Use --data-dir <path> to specify a writable location.");
            return false;
        }
    }

    fs::remove(probe, err); 
    return true;
}

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
        << "  --massive-init            Create a massive initial database of 10,000 vehicles if empty\n"
        << "  --help, -h                Show this message\n"
        << "\nData directory (default):\n"
#if defined(NDEBUG)
        << "  Release build:  $XDG_DATA_HOME/VehicleManagementSystem\n"
        << "                  (~/.local/share/VehicleManagementSystem if XDG_DATA_HOME unset)\n"
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

auto AppPaths::loadConfigFile(ConfigArgs& out_args) -> bool {
    const fs::path config_path = fs::current_path() / "vrs.conf";
    std::ifstream file(config_path);
    if (!file.is_open()) {
        return true; 
    }

    static const std::regex comment_regex(R"(^\s*(?:#.*)?$)");
    static const std::regex kv_regex(R"(^\s*([A-Za-z0-9_-]+)\s*=\s*(.*))");
    std::string line;
    size_t line_num = 0;
    while (std::getline(file, line)) {
        line_num++;
        if (std::regex_match(line, comment_regex)) {
            continue;
        }

        std::smatch match;
        if (!std::regex_match(line, match, kv_regex)) {
            LOG_ERROR("Malformed line in vrs.conf:" + std::to_string(line_num) + ": \"" + line + "\"\n"
                      "        Expected 'key=value'.");
            return false;
        }

        std::string key = match[1].str();
        std::string val = trim(match[2].str());

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
            LOG_ERROR("Unknown configuration key in vrs.conf:" + std::to_string(line_num) + ": \"" + key + "\"");
            return false;
        }
    }

    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto AppPaths::parseSingleArg(std::string_view arg,
                              int& idx,
                              int argc,
                              char** argv,
                              ConfigArgs& out_args) -> bool {
    if (arg == "--local") {
        out_args.data_override = fs::path("data");
        return true;
    }
    if (arg == "--data-dir") {
        if (idx + 1 >= argc) {
            LOG_ERROR("--data-dir requires a path argument.\n"
                      "        Example: --data-dir /home/user/fleet-data");
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

    if (arg == "--storage") {
        if (idx + 1 >= argc) {
            LOG_ERROR("--storage requires a backend name.\n"
                      "        Available: " + join(StorageRegistry::available(), ", "));
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

    if (arg == "--printer") {
        if (idx + 1 >= argc) {
            LOG_ERROR("--printer requires a backend name.\n"
                      "        Available: " + join(PrinterRegistry::available(), ", "));
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
            LOG_ERROR("--printer-device requires an address argument.\n"
                      "        Example: --printer-device 192.168.1.50:9100");
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

    if (arg == "--massive-init") {
        out_args.massive_init = true;
        return true;
    }

    if (arg == "--help" || arg == "-h") {
        return true;
    }

    if (arg.starts_with("-")) {
        LOG_ERROR("Unknown flag: \"" + std::string(arg) + "\"");
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

    if (!loadConfigFile(args)) {
        return false;
    }

    if (!parseCommandLine(argc, argv, args)) {
        return false;
    }

    const std::string resolved_storage =
        args.storage_arg.empty() ? StorageRegistry::defaultName() : args.storage_arg;

    if (!StorageRegistry::contains(resolved_storage)) {
        LOG_ERROR("Unknown storage backend: \"" + resolved_storage + "\"\n"
                  "        Available: " + join(StorageRegistry::available(), ", "));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        printHelp(argv[0], std::cerr);
        return false;
    }

    const std::string resolved_printer =
        args.printer_arg.empty() ? PrinterRegistry::defaultName() : args.printer_arg;

    if (!PrinterRegistry::contains(resolved_printer)) {
        LOG_ERROR("Unknown printer backend: \"" + resolved_printer + "\"\n"
                  "        Available: " + join(PrinterRegistry::available(), ", "));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        printHelp(argv[0], std::cerr);
        return false;
    }

    const fs::path candidate = args.data_override.value_or(platformDefaultDataDir());
    const std::string resolved_device = args.device_arg.empty() ? std::string(DEFAULT_PRINTER_DEVICE) : args.device_arg;

    if (auto err_msg = StorageRegistry::validate(resolved_storage, candidate.string())) {
        LOG_ERROR(*err_msg);
        return false;
    }

    if (auto err_msg = PrinterRegistry::validate(resolved_printer, resolved_device)) {
        LOG_ERROR(*err_msg);
        return false;
    }

    if (!validateAndCreate(candidate)) {
        return false;
    }

    data_dir_       = candidate;
    storage_name_   = resolved_storage;
    printer_name_   = resolved_printer;
    printer_device_ = resolved_device;
    massive_init_   = args.massive_init;

    return true;
}

auto AppPaths::dataDir()       -> const fs::path&   { return data_dir_; }
auto AppPaths::storageName()   -> const std::string& { return storage_name_; }
auto AppPaths::printerName()   -> const std::string& { return printer_name_; }
auto AppPaths::printerDevice() -> const std::string& { return printer_device_; }
auto AppPaths::massiveInit()   -> bool { return massive_init_; }
