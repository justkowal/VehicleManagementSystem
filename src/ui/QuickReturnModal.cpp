#include "ui/QuickReturnModal.h"
#include "notui/Label.h"
#include "notui/HBox.h"
#include "notui/Spacer.h"
#include "notui/Button.h"

using namespace notui;

QuickReturnModal::QuickReturnModal(const std::function<void(std::string, std::string)>& callback)
    : Modal("Quick Return", 50, 11) {
    add_child(std::make_shared<Label>("Enter active rental code:", Size{1, 30}));
    code_in = std::make_shared<InputBox<std::string>>("RENT-XXXXXX", 24);
    add_child(code_in);

    add_child(std::make_shared<Label>("Enter return notes (optional):", Size{1, 30}));
    notes_in = std::make_shared<InputBox<std::string>>("Notes...", 30);
    add_child(notes_in);

    auto btn_row = std::make_shared<HBox>();
    btn_row->fixed_height = 1;
    btn_row->main_axis_alignment = MainAxisAlignment::Center;

    auto ret_btn = std::make_shared<Button>(
        "Return", [this, callback]() { callback(code_in->get_value(), notes_in->get_value()); });
    ret_btn->style.bg({220, 160, 20}).fg({0, 0, 0}).attr(NCSTYLE_BOLD);
    btn_row->add_child(ret_btn);

    auto spacer = std::make_shared<Spacer>();
    spacer->fixed_width = 4;
    spacer->flex = 0;
    btn_row->add_child(spacer);

    cancel_btn = std::make_shared<Button>("Cancel", [=]() { callback("", ""); });
    cancel_btn->style.bg({120, 40, 40}).fg({255, 255, 255});
    btn_row->add_child(cancel_btn);

    add_child(btn_row);
}
