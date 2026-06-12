#include "Vehicle.h"
#include "Exceptions.h"

namespace {
void validateBrandAndModel(const std::string& brand, const std::string& model_or_type) {
    auto is_invalid_str = [](const std::string& s) {
        if (s.empty()) return true;
        for (char c : s) {
            if (c < 32 || c > 126 || c == ',') {
                return true;
            }
        }
        return false;
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
    const auto& c = std::get<Car>(storage_);
    if (c.id == 0) {
        throw ValidationException("Vehicle ID must be greater than 0.");
    }
    validateBrandAndModel(c.brand, c.model);
    if (c.price_per_hour <= 0) {
        throw ValidationException("Rental price per hour must be greater than 0.");
    }
    if (c.seats == 0) {
        throw ValidationException("Car seats must be greater than 0.");
    }
}

Vehicle::Vehicle(Bike bike) : storage_(std::move(bike)) {
    const auto& b = std::get<Bike>(storage_);
    if (b.id == 0) {
        throw ValidationException("Vehicle ID must be greater than 0.");
    }
    validateBrandAndModel(b.brand, b.type);
    if (b.price_per_hour <= 0) {
        throw ValidationException("Rental price per hour must be greater than 0.");
    }
}

Vehicle::Vehicle(Truck truck) : storage_(std::move(truck)) {
    const auto& t = std::get<Truck>(storage_);
    if (t.id == 0) {
        throw ValidationException("Vehicle ID must be greater than 0.");
    }
    validateBrandAndModel(t.brand, t.model);
    if (t.price_per_hour <= 0) {
        throw ValidationException("Rental price per hour must be greater than 0.");
    }
    if (t.payload_capacity_kg == 0) {
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