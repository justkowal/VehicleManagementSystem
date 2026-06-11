#pragma once

#include "IStorage.h"
#include "PluginRegistry.h"

// ---------------------------------------------------------------------------
// StorageRegistry
//
// Type alias of PluginRegistry for IStorage implementations.
// Factory signature: std::unique_ptr<IStorage>(const std::string& data_dir)
// ---------------------------------------------------------------------------
using StorageRegistry = PluginRegistry<IStorage>;

// ---------------------------------------------------------------------------
// REGISTER_STORAGE(name, Type, is_default)
//
// Place this macro at the bottom of a storage implementation .cpp file.
// It registers the type at static-initialisation time — before main() runs.
//
//   name        Lowercase key used for the --storage flag, e.g. "file"
//   Type        The concrete class name, e.g. FileStorage
//   is_default  Pass 'true' for the implementation that is used when the
//               --storage flag is omitted
//
// Example:
//   REGISTER_STORAGE("file", FileStorage, true)
//
// NOTE: Dead-code elimination — static libraries may drop translation units
// not referenced by other code. Since all current implementations are also
// used directly, this is not a concern today. Future stand-alone
// implementations (linked only through the registry) must be listed
// explicitly in CMakeLists.txt or linked with --whole-archive.
// ---------------------------------------------------------------------------
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
