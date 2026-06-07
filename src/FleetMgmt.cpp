#include "FleetMgmt.h"
#include "Types.h"
#include <random>
#include <algorithm>
#include <stdexcept>

FleetManager::FleetManager(std::unique_ptr<IStorage> storage, std::unique_ptr<IPrinter> printer,
                         std::function<std::chrono::system_clock::time_point()> now_fn)
    : storage_(std::move(storage)), printer_(std::move(printer)), now_fn_(std::move(now_fn)) {
    if (!storage_) {
        throw std::invalid_argument("Storage cannot be null");
    }
    // load fleet
    fleet_ = storage_->loadFleet();
}

auto FleetManager::getFleet() const -> std::vector<Vehicle> {
    std::scoped_lock lock(mutex_);
    return fleet_;
}
auto FleetManager::getVehicle(uint32_t vehicle_id) const -> std::optional<Vehicle> {
    std::scoped_lock lock(mutex_);
    auto iter = std::find_if(fleet_.begin(), fleet_.end(), [vehicle_id](const Vehicle& veh) {
        return veh.getId() == vehicle_id;
    });

    if (iter != fleet_.end()) {
        return *iter;
    }
    return std::nullopt;
}

auto FleetManager::addVehicle(const Vehicle& vehicle) -> void {
    std::scoped_lock lock(mutex_);
    if (std::any_of(fleet_.begin(), fleet_.end(), [&](const Vehicle& veh){ return veh.getId() == vehicle.getId(); })) {
        throw std::runtime_error("Vehicle with this ID already exists");
    }
    fleet_.push_back(vehicle);
    storage_->saveFleet(fleet_);
}

auto FleetManager::rentVehicle(uint32_t vehicle_id) -> std::optional<std::string> {
    // lock
    std::string rental_code;
    std::string print_name;
    IPrinter* printer_ptr = nullptr;

    {
        std::scoped_lock lock(mutex_);
        auto iter = std::find_if(fleet_.begin(), fleet_.end(), [vehicle_id](const Vehicle& veh) {
            return veh.getId() == vehicle_id;
        });
        if (iter == fleet_.end() || iter->getStatus() != VehicleStatus::Available) {
            return std::nullopt;
        }

        // mark rented
        iter->setStatus(VehicleStatus::Rented);

        // gen code
        while (true) {
            rental_code = generateRentalCode();
            if (!active_rentals_.contains(rental_code)){
                break;
            }
        }

        active_rentals_[rental_code] = {
            .vehicle_id = vehicle_id,
            .start_time = now_fn_()
        };

        // save while locked
        storage_->saveFleet(fleet_);

        Record record{
            .vehicle_id = vehicle_id,
            .timestamp = now_fn_(),
            .type = RecordType::Checkout,
            .details = "Rented with code: " + rental_code
        };
        storage_->appendRecord(record);

        // prepare printer
        if (printer_ != nullptr) {
            printer_ptr = printer_.get();
            print_name = iter->getBrand() + " " + iter->getModelOrType();
        }
    }

    if (printer_ptr != nullptr) {
        printer_ptr->printCheckout(print_name, rental_code);
    }

    return rental_code;
}

auto FleetManager::returnVehicle(uint32_t vehicle_id) -> bool {
    std::string found_code;
    {
        std::scoped_lock lock(mutex_);
        for (const auto& [rental_code, session] : active_rentals_) {
            if (session.vehicle_id == vehicle_id) {
                found_code = rental_code;
                break;
            }
        }
    }
    if (found_code.empty()) {
        return false;
    }
    return returnVehicle(found_code);
}

auto FleetManager::returnVehicle(const std::string& rental_code) -> bool {
    IPrinter* printer_ptr = nullptr;
    std::string print_name;
    double price_per_hour = 0.0;
    double hours_ceil = 0.0;
    double total_cost = 0.0;

    {
        std::scoped_lock lock(mutex_);
        auto session_it = active_rentals_.find(rental_code);
        if (session_it == active_rentals_.end()) {
            return false;
        }

        const auto session = session_it->second; // copy
        auto iter = std::find_if(fleet_.begin(), fleet_.end(), [&](const Vehicle& veh){ return veh.getId() == session.vehicle_id; });
        if (iter == fleet_.end()) {
            return false;
        }

        // mark available
        iter->setStatus(VehicleStatus::Available);

        auto now = now_fn_();
        std::chrono::duration<double, std::ratio<3600>> hours = now - session.start_time;
        double billable_hours = std::max(1.0, hours.count());
        hours_ceil = std::ceil(billable_hours);
        total_cost = iter->calculateRentalCost(static_cast<uint32_t>(hours_ceil));
        price_per_hour = iter->getPricePerHour();

        // save
        if (storage_) {
            storage_->saveFleet(fleet_);
            Record log{
                .vehicle_id = session.vehicle_id,
                .timestamp = now,
                .type = RecordType::Returned,
                .details = "Returned. Duration: " + std::to_string(billable_hours) + "h. Total: $" + std::to_string(total_cost)
            };
            storage_->appendRecord(log);
        }

        // prepare printer
        if (printer_ != nullptr) {
            printer_ptr = printer_.get();
            print_name = iter->getBrand() + " " + iter->getModelOrType();
        }

        // erase rental
        active_rentals_.erase(session_it);
    }

    if (printer_ptr != nullptr) {
        printer_ptr->printReturn(print_name, price_per_hour, hours_ceil, total_cost);
    }

    return true;
}

auto FleetManager::generateRentalCode() -> std::string {
    static const std::string alphanum = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rnd;
    std::mt19937 gen(rnd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(alphanum.size()) - 1);

    std::string code = "RENT-";
    for (int i = 0; i < 6; ++i) {
        code += alphanum[dis(gen)];
    }
    return code;
}