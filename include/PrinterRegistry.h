#pragma once

#include "IPrinter.h"
#include "PluginRegistry.h"

// printer registry (type alias)
using PrinterRegistry = PluginRegistry<IPrinter>;

// registers printer at static initialization time
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
