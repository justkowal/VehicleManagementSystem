#pragma once

#include "IPrinter.h"
#include "PluginRegistry.h"

// ---------------------------------------------------------------------------
// PrinterRegistry
//
// Type alias of PluginRegistry for IPrinter implementations.
// Factory signature: std::unique_ptr<IPrinter>(const std::string& device)
// The config string is the device address / path, e.g. "127.0.0.1:9100".
// ---------------------------------------------------------------------------
using PrinterRegistry = PluginRegistry<IPrinter>;

// ---------------------------------------------------------------------------
// REGISTER_PRINTER(name, Type, is_default)
//
// Place this macro at the bottom of a printer implementation .cpp file.
// It registers the type at static-initialisation time — before main() runs.
//
//   name        Lowercase key used for the --printer flag, e.g. "escpos"
//   Type        The concrete class name, e.g. EscPosPrinter
//   is_default  Pass 'true' for the implementation that is used when the
//               --printer flag is omitted
//
// Example:
//   REGISTER_PRINTER("escpos", EscPosPrinter, true)
// ---------------------------------------------------------------------------
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-macro-usage)
#define REGISTER_PRINTER(name, Type, is_default)                                    \
    static const bool _printer_registered_##Type =                                  \
        PrinterRegistry::add(                                                        \
            (name),                                                                  \
            [](const std::string& config) -> std::unique_ptr<IPrinter> {            \
                return std::make_unique<Type>(config);                              \
            },                                                                       \
            (is_default))

#define REGISTER_PRINTER_WITH_VALIDATOR(name, Type, validator, is_default)          \
    static const bool _printer_registered_##Type =                                  \
        PrinterRegistry::add(                                                        \
            (name),                                                                  \
            [](const std::string& config) -> std::unique_ptr<IPrinter> {            \
            return std::make_unique<Type>(config);                                  \
            },                                                                       \
            (validator),                                                             \
            (is_default))
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-macro-usage)
