#include "ui/RegisterVehicleModal.h"
#include "notui/Dropdown.h"
#include "notui/InputBox.h"
#include "notui/Label.h"
#include "notui/HBox.h"
#include "notui/Spacer.h"
#include "notui/Button.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>

using namespace notui;

RegisterVehicleModal::RegisterVehicleModal(
    const std::function<void(std::optional<Vehicle>)>& callback)
    : Modal("Register Vehicle", 50, 15) {
    auto type_select = std::make_shared<Dropdown>(std::vector<std::string>{"Car", "Bike", "Truck"});
    auto brand_in = std::make_shared<InputBox<std::string>>("Brand...", 30);
    auto model_in = std::make_shared<InputBox<std::string>>("Model/Type...", 30);
    auto rate_in =
        std::make_shared<InputBox<double>>("Rate PLN/hr...", 30, [](const std::string& str) {
            if (str.empty()) {
                return true;
            }
            try {
                std::stod(str);
                return true;
            } catch (...) {
                return false;
            }
        });
    auto spec_in =
        std::make_shared<InputBox<int>>("Seats/Capacity...", 30, [](const std::string& str) {
            return str.empty() || std::all_of(str.begin(), str.end(), [](unsigned char character) {
                       return std::isdigit(character) != 0;
                   });
        });

    auto model_lbl = std::make_shared<Label>("Model Name:", Size{1, 24});
    auto spec_lbl = std::make_shared<Label>("Seats Count:", Size{1, 38});

    type_select->on("change", [=](const Event&) {
        int idx = type_select->get_selected_index();
        if (idx == 0) {
            model_lbl->set_text("Model Name:");
            spec_lbl->set_text("Seats Count:");
            spec_in->focusable = true;
            spec_in->set_value("");
        } else if (idx == 1) {
            model_lbl->set_text("Category Name:");
            spec_in->focusable = false;
            spec_in->set_value("N/A");
            spec_lbl->set_text("N/A");
        } else {
            model_lbl->set_text("Model Name:");
            spec_lbl->set_text("Payload Capacity (kg):");
            spec_in->focusable = true;
            spec_in->set_value("");
        }
    });

    add_child(std::make_shared<Label>("Vehicle Type:", Size{1, 15}));
    add_child(type_select);
    add_child(std::make_shared<Label>("Brand Name:", Size{1, 15}));
    add_child(brand_in);
    add_child(model_lbl);
    add_child(model_in);
    add_child(std::make_shared<Label>("Rental Rate per Hour (PLN):", Size{1, 28}));
    add_child(rate_in);
    add_child(spec_lbl);
    add_child(spec_in);

    auto btn_row = std::make_shared<HBox>();
    btn_row->fixed_height = 1;
    btn_row->main_axis_alignment = MainAxisAlignment::Center;

    auto save_btn = std::make_shared<Button>("Save", [=]() {
        std::string brand = brand_in->get_value();
        std::string model = model_in->get_value();
        double rate_val = rate_in->get_value();
        int spec = spec_in->get_value();

        if (brand.empty() || model.empty() || rate_val <= 0.0) {
            return;
        }

        int rate = static_cast<int>(std::round(rate_val * 100.0));
        uint32_t random_id = 200 + static_cast<uint32_t>(std::rand() % 9700);
        Vehicle new_veh(Car{});
        int idx = type_select->get_selected_index();
        if (idx == 0) {
            new_veh =
                Vehicle(Car{random_id, brand, model, static_cast<uint8_t>(spec > 0 ? spec : 5),
                            rate, VehicleStatus::Available});
        } else if (idx == 1) {
            new_veh = Vehicle(Bike{random_id, brand, model, rate, VehicleStatus::Available});
        } else {
            new_veh = Vehicle(Truck{random_id, brand, model,
                                    static_cast<uint32_t>(spec > 0 ? spec : 2000), rate,
                                    VehicleStatus::Available});
        }
        callback(new_veh);
    });
    save_btn->style.bg({40, 120, 60}).fg({255, 255, 255});
    btn_row->add_child(save_btn);

    auto sp_btn = std::make_shared<Spacer>();
    sp_btn->fixed_width = 4;
    sp_btn->flex = 0;
    btn_row->add_child(sp_btn);

    cancel_btn = std::make_shared<Button>("Cancel", [=]() { callback(std::nullopt); });
    cancel_btn->style.bg({120, 40, 40}).fg({255, 255, 255});
    btn_row->add_child(cancel_btn);

    add_child(btn_row);
}
