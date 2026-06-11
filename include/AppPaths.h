#pragma once

#include <filesystem>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

class AppPaths {
public:
    struct ConfigArgs {
        std::optional<std::filesystem::path> data_override;
        std::string storage_arg;
        std::string printer_arg;
        std::string device_arg;
    };

    AppPaths() = delete;

    // Parses argc/argv, resolves all config, creates the data directory, and
    // validates it is writable. Returns false and prints a diagnostic to
    // stderr on any error — callers should exit(EXIT_FAILURE) on false.
    [[nodiscard]] static auto resolve(int argc, char** argv) -> bool;

    // Prints usage (including dynamically-listed backends) and returns true
    // when --help / -h was passed. Callers should exit(EXIT_SUCCESS) on true.
    [[nodiscard]] static auto handleHelp(int argc, char** argv) -> bool;

    // Prints the general help message. Defaults to std::cout.
    static auto printHelp(const char* prog_name, std::ostream& out_stream = std::cout) -> void;

    // ---- Getters (undefined behaviour if called before resolve()) ----------

    [[nodiscard]] static auto dataDir()       -> const std::filesystem::path&;
    [[nodiscard]] static auto storageName()   -> const std::string&;
    [[nodiscard]] static auto printerName()   -> const std::string&;
    [[nodiscard]] static auto printerDevice() -> const std::string&;

private:
    static auto platformDefaultDataDir() -> std::filesystem::path;
    static auto validateAndCreate(const std::filesystem::path& path) -> bool;

    // Helper to load configuration file from PWD.
    [[nodiscard]] static auto loadConfigFile(ConfigArgs& out_args) -> bool;

    // Helper to parse CLI arguments.
    [[nodiscard]] static auto parseCommandLine(int argc,
                                               char** argv,
                                               ConfigArgs& out_args) -> bool;

    // Helper to parse a single CLI argument.
    [[nodiscard]] static auto parseSingleArg(std::string_view arg,
                                             int& idx,
                                             int argc,
                                             char** argv,
                                             ConfigArgs& out_args) -> bool;

    // Storage for resolved config — populated by resolve().
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::filesystem::path data_dir_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string storage_name_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string printer_name_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string printer_device_;
};
