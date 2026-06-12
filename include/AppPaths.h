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

    // parse and resolve config
    [[nodiscard]] static auto resolve(int argc, char** argv) -> bool;

    // print help if -h/--help passed
    [[nodiscard]] static auto handleHelp(int argc, char** argv) -> bool;

    // print general help message
    static auto printHelp(const char* prog_name, std::ostream& out_stream = std::cout) -> void;

    // getters

    [[nodiscard]] static auto dataDir()       -> const std::filesystem::path&;
    [[nodiscard]] static auto storageName()   -> const std::string&;
    [[nodiscard]] static auto printerName()   -> const std::string&;
    [[nodiscard]] static auto printerDevice() -> const std::string&;

private:
    static auto platformDefaultDataDir() -> std::filesystem::path;
    static auto validateAndCreate(const std::filesystem::path& path) -> bool;

    // load config from file
    [[nodiscard]] static auto loadConfigFile(ConfigArgs& out_args) -> bool;

    // parse cli arguments
    [[nodiscard]] static auto parseCommandLine(int argc,
                                               char** argv,
                                               ConfigArgs& out_args) -> bool;

    // parse single cli argument
    [[nodiscard]] static auto parseSingleArg(std::string_view arg,
                                             int& idx,
                                             int argc,
                                             char** argv,
                                             ConfigArgs& out_args) -> bool;

    // resolved config storage
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::filesystem::path data_dir_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string storage_name_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string printer_name_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string printer_device_;
};
