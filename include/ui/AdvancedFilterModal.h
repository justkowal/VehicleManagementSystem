#pragma once

#include "notui/Modal.h"
#include "notui/InputBox.h"
#include "Vehicle.h"
#include <string>
#include <functional>
#include <memory>
#include <optional>

class AdvancedFilterModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> id_min;
    std::shared_ptr<notui::InputBox<std::string>> id_max;
    std::shared_ptr<notui::InputBox<std::string>> id_eq;

    std::shared_ptr<notui::InputBox<std::string>> brand_eq;

    std::shared_ptr<notui::InputBox<std::string>> model_eq;

    std::shared_ptr<notui::InputBox<std::string>> type_eq;

    std::shared_ptr<notui::InputBox<std::string>> status_eq;

    std::shared_ptr<notui::InputBox<std::string>> price_min;
    std::shared_ptr<notui::InputBox<std::string>> price_max;
    std::shared_ptr<notui::InputBox<std::string>> price_eq;

    std::shared_ptr<notui::InputBox<std::string>> seats_min;
    std::shared_ptr<notui::InputBox<std::string>> seats_max;
    std::shared_ptr<notui::InputBox<std::string>> seats_eq;

    std::shared_ptr<notui::InputBox<std::string>> payload_min;
    std::shared_ptr<notui::InputBox<std::string>> payload_max;
    std::shared_ptr<notui::InputBox<std::string>> payload_eq;

    std::shared_ptr<notui::InputBox<std::string>> rental_code_eq;

public:
    AdvancedFilterModal(const AdvancedFilter& current_filter, const std::function<void(std::optional<AdvancedFilter>)>& callback);
};
