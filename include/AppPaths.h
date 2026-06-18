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
        bool massive_init = false;
    };

    AppPaths() = delete;

    [[nodiscard]] static auto resolve(int argc, char** argv) -> bool;

    [[nodiscard]] static auto handleHelp(int argc, char** argv) -> bool;

    static auto printHelp(const char* prog_name, std::ostream& out_stream = std::cout) -> void;


    [[nodiscard]] static auto dataDir()       -> const std::filesystem::path&;
    [[nodiscard]] static auto storageName()   -> const std::string&;
    [[nodiscard]] static auto printerName()   -> const std::string&;
    [[nodiscard]] static auto printerDevice() -> const std::string&;
    [[nodiscard]] static auto massiveInit()   -> bool;

private:
    static auto platformDefaultDataDir() -> std::filesystem::path;
    static auto validateAndCreate(const std::filesystem::path& path) -> bool;

    [[nodiscard]] static auto loadConfigFile(ConfigArgs& out_args) -> bool;

    [[nodiscard]] static auto parseCommandLine(int argc,
                                               char** argv,
                                               ConfigArgs& out_args) -> bool;

    [[nodiscard]] static auto parseSingleArg(std::string_view arg,
                                             int& idx,
                                             int argc,
                                             char** argv,
                                             ConfigArgs& out_args) -> bool;

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::filesystem::path data_dir_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string storage_name_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string printer_name_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static std::string printer_device_;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static bool massive_init_;
};
