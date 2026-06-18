#pragma once

#include "notui/Modal.h"
#include "notui/InputBox.h"
#include "Vehicle.h"
#include <string>
#include <functional>
#include <memory>

class RentVehicleModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> name_in;
    std::shared_ptr<notui::InputBox<std::string>> surname_in;
    std::shared_ptr<notui::InputBox<std::string>> id_card_in;
public:
    RentVehicleModal(const Vehicle& vehicle, const std::function<void(bool, std::string, std::string, std::string)>& callback);
};
