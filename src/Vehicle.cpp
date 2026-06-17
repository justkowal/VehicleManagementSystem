#include "Vehicle.h"
#include "Exceptions.h"
#include <regex>

namespace {
void validateBrandAndModel(const std::string& brand, const std::string& model_or_type) {
    static const std::regex valid_regex(R"(^[\x20-\x2b\x2d-\x7e]+$)");
    auto is_invalid_str = [](const std::string& str_val) {
        return !std::regex_match(str_val, valid_regex);
    };
    if (is_invalid_str(brand)) {
        throw ValidationException("Invalid brand: '" + brand + "'. Must be non-empty, printable ASCII, and cannot contain commas or newlines.");
    }
    if (is_invalid_str(model_or_type)) {
        throw ValidationException("Invalid model/type: '" + model_or_type + "'. Must be non-empty, printable ASCII, and cannot contain commas or newlines.");
    }
}
} // namespace

Vehicle::Vehicle(Car car) : storage_(std::move(car)) {
    const auto& car_obj = std::get<Car>(storage_);
    if (car_obj.id == 0) {
        throw ValidationException("Vehicle ID must be greater than 0.");
    }
    validateBrandAndModel(car_obj.brand, car_obj.model);
    if (car_obj.price_per_hour <= 0) {
        throw ValidationException("Rental price per hour must be greater than 0.");
    }
    if (car_obj.seats == 0) {
        throw ValidationException("Car seats must be greater than 0.");
    }
}

Vehicle::Vehicle(Bike bike) : storage_(std::move(bike)) {
    const auto& bike_obj = std::get<Bike>(storage_);
    if (bike_obj.id == 0) {
        throw ValidationException("Vehicle ID must be greater than 0.");
    }
    validateBrandAndModel(bike_obj.brand, bike_obj.type);
    if (bike_obj.price_per_hour <= 0) {
        throw ValidationException("Rental price per hour must be greater than 0.");
    }
}

Vehicle::Vehicle(Truck truck) : storage_(std::move(truck)) {
    const auto& truck_obj = std::get<Truck>(storage_);
    if (truck_obj.id == 0) {
        throw ValidationException("Vehicle ID must be greater than 0.");
    }
    validateBrandAndModel(truck_obj.brand, truck_obj.model);
    if (truck_obj.price_per_hour <= 0) {
        throw ValidationException("Rental price per hour must be greater than 0.");
    }
    if (truck_obj.payload_capacity_kg == 0) {
        throw ValidationException("Truck payload capacity must be greater than 0.");
    }
}


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