#include "ui/RentVehicleModal.h"
#include "ui/UIHelpers.h"
#include "notui/Label.h"
#include "notui/HBox.h"
#include "notui/Spacer.h"
#include "notui/Button.h"

using namespace notui;

RentVehicleModal::RentVehicleModal(
    const Vehicle& vehicle,
    const std::function<void(bool, std::string, std::string, std::string)>& callback)
    : Modal("Confirm Rental", 50, 13) {

    std::string details = "Renting: " + vehicle.getBrand() + " " + vehicle.getModelOrType() +
                          "\nRate: " + ui_helpers::uiFormatMoney(vehicle.getPricePerHour()) + " PLN/h";
    auto details_lbl = std::make_shared<Label>(details, Size{2, 46}, true);
    details_lbl->style.fg({200, 200, 200});
    add_child(details_lbl);

    add_child(std::make_shared<Label>("Renter Name:", Size{1, 15}));
    name_in = std::make_shared<InputBox<std::string>>("First Name...", 30);
    add_child(name_in);

    add_child(std::make_shared<Label>("Renter Surname:", Size{1, 15}));
    surname_in = std::make_shared<InputBox<std::string>>("Last Name...", 30);
    add_child(surname_in);

    add_child(std::make_shared<Label>("Renter ID Card Number:", Size{1, 24}));
    id_card_in = std::make_shared<InputBox<std::string>>("ID Card...", 30);
    add_child(id_card_in);

    auto btn_row = std::make_shared<HBox>();
    btn_row->fixed_height = 1;
    btn_row->main_axis_alignment = MainAxisAlignment::Center;

    auto confirm_btn = std::make_shared<Button>("Confirm", [this, callback]() {
        callback(true, name_in->get_value(), surname_in->get_value(), id_card_in->get_value());
    });
    confirm_btn->style.bg({40, 120, 60}).fg({255, 255, 255});
    btn_row->add_child(confirm_btn);

    auto spacer = std::make_shared<Spacer>();
    spacer->fixed_width = 4;
    spacer->flex = 0;
    btn_row->add_child(spacer);

    cancel_btn = std::make_shared<Button>("Cancel", [=]() { callback(false, "", "", ""); });
    cancel_btn->style.bg({120, 40, 40}).fg({255, 255, 255});
    btn_row->add_child(cancel_btn);

    add_child(btn_row);
}
