#pragma once

#include "IStorage.h"
#include "PluginRegistry.h"

// storage registry (type alias)
using StorageRegistry = PluginRegistry<IStorage>;

// registers storage at static initialization time
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-macro-usage)
#define REGISTER_STORAGE(name, Type, is_default)                                    \
    static const bool _storage_registered_##Type =                                  \
        StorageRegistry::add(                                                        \
            (name),                                                                  \
            [](const std::string& config) -> std::unique_ptr<IStorage> {            \
                return std::make_unique<Type>(config);                              \
            },                                                                       \
            (is_default))

#define REGISTER_STORAGE_WITH_VALIDATOR(name, Type, validator, is_default)          \
    static const bool _storage_registered_##Type =                                  \
        StorageRegistry::add(                                                        \
            (name),                                                                  \
            [](const std::string& config) -> std::unique_ptr<IStorage> {            \
                return std::make_unique<Type>(config);                              \
            },                                                                       \
            (validator),                                                             \
            (is_default))
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,cppcoreguidelines-macro-usage)
