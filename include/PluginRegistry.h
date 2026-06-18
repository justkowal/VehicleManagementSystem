#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

template<typename Interface>
class PluginRegistry {
public:
    using Factory = std::function<std::unique_ptr<Interface>(const std::string&)>;
    using Validator = std::function<std::optional<std::string>(const std::string&)>;

    struct RegistryEntry {
        Factory factory;
        Validator validator;
    };

    static auto add(std::string name,
                    Factory factory,
                    Validator validator = nullptr,
                    bool is_default = false) -> bool {
        auto& entries = table();
        if (is_default || entries.empty()) {
            defaultKey() = name;
        }
        entries.insert_or_assign(name, RegistryEntry{std::move(factory), std::move(validator)});
        return true;
    }

    static auto add(std::string name, Factory factory, bool is_default) -> bool {
        return add(std::move(name), std::move(factory), nullptr, is_default);
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] static auto create(const std::string& name,
                                     const std::string& config) -> std::unique_ptr<Interface> {
        auto& entries = table();
        auto iter = entries.find(name);
        if (iter == entries.end()) {
            return nullptr;
        }
        return iter->second.factory(config);
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    [[nodiscard]] static auto validate(const std::string& name,
                                       const std::string& config) -> std::optional<std::string> {
        auto& entries = table();
        auto iter = entries.find(name);
        if (iter == entries.end()) {
            return "Implementation '" + name + "' not found.";
        }
        if (iter->second.validator) {
            return iter->second.validator(config);
        }
        return std::nullopt;
    }

    [[nodiscard]] static auto available() -> std::vector<std::string> {
        std::vector<std::string> names;
        names.reserve(table().size());
        for (const auto& [name, _] : table()) {
            names.push_back(name);
        }
        return names;
    }

    [[nodiscard]] static auto contains(const std::string& name) -> bool {
        return table().contains(name);
    }

    [[nodiscard]] static auto defaultName() -> const std::string& {
        return defaultKey();
    }

private:
    static auto table() -> std::map<std::string, RegistryEntry>& {
        static std::map<std::string, RegistryEntry> instance;
        return instance;
    }

    static auto defaultKey() -> std::string& {
        static std::string instance;
        return instance;
    }
};
