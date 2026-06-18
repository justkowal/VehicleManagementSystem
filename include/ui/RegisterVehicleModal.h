#pragma once

#include "notui/Modal.h"
#include "Vehicle.h"
#include <functional>
#include <optional>

class RegisterVehicleModal : public notui::Modal {
public:
    explicit RegisterVehicleModal(const std::function<void(std::optional<Vehicle>)>& callback);
};
