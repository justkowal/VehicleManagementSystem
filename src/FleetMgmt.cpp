#include "FleetMgmt.h"
#include "Types.h"
#include "Exceptions.h"
#include "Logger.h"
#include <algorithm>
#include <random>
#include <stdexcept>
#include <fstream>
#include <cmath>

namespace {
bool isAlphaSpaceOnly(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ' ')) {
            return false;
        }
    }
    return true;
}

bool isAlnumOnly(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            return false;
        }
    }
    return true;
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void validateCustomerDetails(const std::string& name, const std::string& surname, const std::string& id_card) {
    if (!isAlphaSpaceOnly(name)) {
        throw ValidationException("Renter name must be non-empty and contain only letters and spaces.");
    }
    if (!isAlphaSpaceOnly(surname)) {
        throw ValidationException("Renter surname must be non-empty and contain only letters and spaces.");
    }
    if (!isAlnumOnly(id_card)) {
        throw ValidationException("Renter ID card number must be non-empty and contain only alphanumeric characters.");
    }
}

void validateNotes(const std::string& notes) {
    if (notes.empty()) return;
    for (char c : notes) {
        if (c < 32 || c > 126 || c == ',') {
            throw ValidationException("Notes cannot contain commas, newlines, or non-printable characters.");
        }
    }
}

bool isValidRentalCode(const std::string& code) {
    if (code.length() != 11) return false;
    if (code.substr(0, 5) != "RENT-") return false;
    for (int i = 5; i < 11; ++i) {
        char c = code[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) {
            return false;
        }
    }
    return true;
}
} // namespace


FleetManager::FleetManager(std::unique_ptr<IStorage> storage, std::unique_ptr<IPrinter> printer,
                           std::function<std::chrono::system_clock::time_point()> now_fn)
    : storage_(std::move(storage)), printer_(std::move(printer)), now_fn_(std::move(now_fn)) {
    if (!storage_) {
        throw std::invalid_argument("Storage cannot be null");
    }
    fleet_ = storage_->loadFleet();
    active_rentals_ = storage_->loadActiveRentals();
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
    if (std::any_of(fleet_.begin(), fleet_.end(),
                    [&](const Vehicle& veh) { return veh.getId() == vehicle.getId(); })) {
        throw VehicleAlreadyExistsException(vehicle.getId());
    }
    auto new_fleet = fleet_;
    new_fleet.push_back(vehicle);
    storage_->saveFleet(new_fleet);
    fleet_ = std::move(new_fleet);
}

auto FleetManager::removeVehicle(uint32_t vehicle_id) -> bool {
    std::scoped_lock lock(mutex_);
    auto iter = std::find_if(fleet_.begin(), fleet_.end(), [vehicle_id](const Vehicle& veh) {
        return veh.getId() == vehicle_id;
    });
    if (iter == fleet_.end()) {
        throw VehicleNotFoundException(vehicle_id);
    }
    if (iter->getStatus() == VehicleStatus::Rented) {
        throw VehicleStatusException("Vehicle " + std::to_string(vehicle_id) + " is currently rented and cannot be decommissioned.");
    }
    auto new_fleet = fleet_;
    auto new_iter = std::find_if(new_fleet.begin(), new_fleet.end(), [vehicle_id](const Vehicle& veh) {
        return veh.getId() == vehicle_id;
    });
    new_fleet.erase(new_iter);
    storage_->saveFleet(new_fleet);
    storage_->deleteRecords(vehicle_id);
    fleet_ = std::move(new_fleet);
    return true;
}

auto FleetManager::rentVehicle(uint32_t vehicle_id, const std::string& name, const std::string& surname, const std::string& id_card, std::string* out_print_warning) -> std::optional<std::string> {
    validateCustomerDetails(name, surname, id_card);

    std::string rental_code;
    std::string print_name;
    IPrinter* printer_ptr = nullptr;
    std::chrono::system_clock::time_point checkout_time;

    {
        std::scoped_lock lock(mutex_);
        auto iter = std::find_if(fleet_.begin(), fleet_.end(), [vehicle_id](const Vehicle& veh) {
            return veh.getId() == vehicle_id;
        });
        if (iter == fleet_.end()) {
            throw VehicleNotFoundException(vehicle_id);
        }
        if (iter->getStatus() != VehicleStatus::Available) {
            throw VehicleStatusException("Vehicle " + std::to_string(vehicle_id) + " is not available for rent (current status: " + iter->getStatusString() + ").");
        }

        auto new_fleet = fleet_;
        auto new_iter = std::find_if(new_fleet.begin(), new_fleet.end(), [vehicle_id](const Vehicle& veh) {
            return veh.getId() == vehicle_id;
        });
        new_iter->setStatus(VehicleStatus::Rented);

        while (true) {
            rental_code = generateRentalCode();
            if (!active_rentals_.contains(rental_code)) {
                break;
            }
        }

        checkout_time = now_fn_();
        auto new_active_rentals = active_rentals_;
        new_active_rentals[rental_code] = {.vehicle_id = vehicle_id, .start_time = checkout_time};

        storage_->saveFleet(new_fleet);
        storage_->saveActiveRentals(new_active_rentals);

        std::string details = "Rented with code: " + rental_code;
        if (!name.empty() || !surname.empty() || !id_card.empty()) {
            details += " by " + name + " " + surname + " (ID: " + id_card + ")";
        }

        Record record{.vehicle_id = vehicle_id,
                      .timestamp = checkout_time,
                      .type = RecordType::Checkout,
                      .details = details};
        storage_->appendRecord(record);

        fleet_ = std::move(new_fleet);
        active_rentals_ = std::move(new_active_rentals);

        if (printer_ != nullptr) {
            printer_ptr = printer_.get();
            print_name = new_iter->getBrand() + " " + new_iter->getModelOrType();
        } else {
            LOG_WARNING("printer_ is NULL!");
        }
    }

    if (printer_ptr != nullptr) {
        try {
            printer_ptr->printCheckout(print_name, rental_code, name, surname, id_card, checkout_time);
        } catch (const PrinterException& e) {
            if (out_print_warning != nullptr) {
                *out_print_warning = e.what();
            }
        }
    }

    return rental_code;
}

auto FleetManager::returnVehicle(uint32_t vehicle_id, int* out_cost, const std::string& notes, std::string* out_print_warning) -> bool {
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
        throw VehicleStatusException("Vehicle " + std::to_string(vehicle_id) + " is not currently rented.");
    }
    return returnVehicle(found_code, out_cost, notes, out_print_warning);
}

auto FleetManager::returnVehicle(const std::string& rental_code, int* out_cost, const std::string& notes, std::string* out_print_warning) -> bool {
    std::string clean_code = rental_code;
    for (char& c : clean_code) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    if (!isValidRentalCode(clean_code)) {
        throw ValidationException("Invalid rental code format. Must be RENT-XXXXXX.");
    }
    validateNotes(notes);

    IPrinter* printer_ptr = nullptr;
    std::string print_name;
    int price_per_hour = 0;
    double hours_ceil = 0.0;
    int total_cost = 0;

    {
        std::scoped_lock lock(mutex_);
        auto session_it = active_rentals_.find(clean_code);
        if (session_it == active_rentals_.end()) {
            throw VehicleStatusException("Rental code " + clean_code + " is not active.");
        }

        const auto session = session_it->second;
        auto iter = std::find_if(fleet_.begin(), fleet_.end(), [&](const Vehicle& veh) {
            return veh.getId() == session.vehicle_id;
        });
        if (iter == fleet_.end()) {
            throw VehicleNotFoundException(session.vehicle_id);
        }

        auto new_fleet = fleet_;
        auto new_iter = std::find_if(new_fleet.begin(), new_fleet.end(), [&](const Vehicle& veh) {
            return veh.getId() == session.vehicle_id;
        });
        new_iter->setStatus(VehicleStatus::Available);

        auto now = now_fn_();
        std::chrono::duration<double, std::ratio<3600>> hours = now - session.start_time;
        double billable_hours = std::max(1.0, hours.count());
        hours_ceil = std::ceil(billable_hours);
        total_cost = new_iter->calculateRentalCost(static_cast<uint32_t>(hours_ceil));
        price_per_hour = new_iter->getPricePerHour();

        if (out_cost != nullptr) {
            *out_cost = total_cost;
        }

        if (storage_) {
            storage_->saveFleet(new_fleet);
            int zlotys = total_cost / 100;
            int groszy = std::abs(total_cost % 100);
            std::ostringstream cost_oss;
            cost_oss << zlotys << "." << std::setw(2) << std::setfill('0') << groszy;
            std::string details = "Returned. Duration: " + std::to_string(billable_hours) +
                                  "h. Total: " + cost_oss.str() + " PLN";
            if (!notes.empty()) {
                details += ". Notes: " + notes;
            }
            Record log{.vehicle_id = session.vehicle_id,
                       .timestamp = now,
                       .type = RecordType::Returned,
                       .details = details};
            storage_->appendRecord(log);
        }

        auto new_active_rentals = active_rentals_;
        new_active_rentals.erase(clean_code);

        if (storage_) {
            storage_->saveActiveRentals(new_active_rentals);
        }

        fleet_ = std::move(new_fleet);
        active_rentals_ = std::move(new_active_rentals);

        if (printer_ != nullptr) {
            printer_ptr = printer_.get();
            print_name = new_iter->getBrand() + " " + new_iter->getModelOrType();
        }
    }

    if (printer_ptr != nullptr) {
        try {
            printer_ptr->printReturn(print_name, price_per_hour, hours_ceil, total_cost);
        } catch (const PrinterException& e) {
            if (out_print_warning != nullptr) {
                *out_print_warning = e.what();
            }
        }
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
        code += alphanum[static_cast<size_t>(dis(gen))];
    }
    return code;
}

auto FleetManager::updateVehicleStatus(uint32_t vehicle_id, VehicleStatus status, const std::string& notes) -> bool {
    validateNotes(notes);
    std::scoped_lock lock(mutex_);
    auto iter = std::find_if(fleet_.begin(), fleet_.end(), [vehicle_id](const Vehicle& veh) {
        return veh.getId() == vehicle_id;
    });
    if (iter == fleet_.end()) {
        throw VehicleNotFoundException(vehicle_id);
    }
    auto new_fleet = fleet_;
    auto new_iter = std::find_if(new_fleet.begin(), new_fleet.end(), [vehicle_id](const Vehicle& veh) {
        return veh.getId() == vehicle_id;
    });
    new_iter->setStatus(status);
    storage_->saveFleet(new_fleet);

    RecordType rec_type = RecordType::Note;
    std::string details;
    if (status == VehicleStatus::Maintenance) {
        rec_type = RecordType::Maintenance;
        details = "Excluded from fleet: Sent to maintenance";
    } else if (status == VehicleStatus::Available) {
        details = "Restored: Marked fixed and available";
    } else {
        details = "Status changed";
    }

    if (!notes.empty()) {
        details += ". Notes: " + notes;
    }

    Record record{.vehicle_id = vehicle_id,
                  .timestamp = now_fn_(),
                  .type = rec_type,
                  .details = std::move(details)};
    storage_->appendRecord(record);
    fleet_ = std::move(new_fleet);
    return true;
}

auto FleetManager::getVehicleIdByRentalCode(const std::string& rental_code) const -> std::optional<uint32_t> {
    std::scoped_lock lock(mutex_);
    auto iter = active_rentals_.find(rental_code);
    if (iter != active_rentals_.end()) {
        return iter->second.vehicle_id;
    }
    return std::nullopt;
}

auto FleetManager::getRentalCodeByVehicleId(uint32_t vehicle_id) const -> std::optional<std::string> {
    std::scoped_lock lock(mutex_);
    for (const auto& [rental_code, session] : active_rentals_) {
        if (session.vehicle_id == vehicle_id) {
            return rental_code;
        }
    }
    return std::nullopt;
}

auto FleetManager::getRentalBilledHours(uint32_t vehicle_id) const -> std::optional<uint32_t> {
    std::scoped_lock lock(mutex_);
    for (const auto& [rental_code, session] : active_rentals_) {
        if (session.vehicle_id == vehicle_id) {
            auto now = now_fn_();
            std::chrono::duration<double, std::ratio<3600>> hours = now - session.start_time;
            double billable_hours = std::max(1.0, hours.count());
            return static_cast<uint32_t>(std::ceil(billable_hours));
        }
    }
    return std::nullopt;
}