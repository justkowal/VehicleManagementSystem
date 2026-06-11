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

auto Vehicle::getTypeString() const -> std::string {
    return std::visit([](const auto& vis) { 
        if constexpr (std::is_same_v<std::decay_t<decltype(vis)>, Car>) {
            return "Car";
        } else if constexpr (std::is_same_v<std::decay_t<decltype(vis)>, Bike>) {
            return "Bike";
        } else {
            return "Truck";
        }
    }, storage_);
}

auto Vehicle::getStatus() const -> VehicleStatus {
    return std::visit([](const auto& vis) { return vis.status; }, storage_);
}

auto Vehicle::getStatusString() const -> std::string {
    return std::visit([](const auto& vis) { 
        switch (vis.status) {
            case VehicleStatus::Available: return "Available";
            case VehicleStatus::Rented: return "Rented";
            case VehicleStatus::Maintenance: return "Maintenance";
            default: return "Unknown";
        }
    }, storage_);
}

auto Vehicle::getPricePerHour() const -> int {
    return std::visit([](const auto& vis) { return vis.price_per_hour; }, storage_);
}

auto Vehicle::setStatus(VehicleStatus new_status) -> void {
    std::visit([new_status](auto& vis) { vis.status = new_status; }, storage_);
}

auto Vehicle::calculateRentalCost(uint32_t hours) const -> int {
    return getPricePerHour() * static_cast<int>(hours);
}

auto Vehicle::getVariant() const -> const VehicleVariant& {
    return storage_;
}

auto Vehicle::getVariant() -> VehicleVariant& {
    return storage_;
}