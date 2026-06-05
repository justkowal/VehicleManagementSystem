#include "Vehicle.h"

Vehicle::Vehicle(Car car) : storage_(std::move(car)) {}
Vehicle::Vehicle(Bike bike) : storage_(std::move(bike)) {}
Vehicle::Vehicle(Truck truck) : storage_(std::move(truck)) {}

auto Vehicle::getId() const -> uint32_t {
    return std::visit([](const auto& vis) { return vis.id; }, storage_);
}

auto Vehicle::getBrand() const -> std::string {
    return std::visit([](const auto& vis) { return vis.brand; }, storage_);
}

auto Vehicle::getModelOrType() const -> std::string {
    return std::visit([](const auto& vis) { 
        if constexpr (std::is_same_v<std::decay_t<decltype(vis)>, Bike>) {
            return vis.type;
        } else { 
            return vis.model;
        }
    }, storage_);
}   