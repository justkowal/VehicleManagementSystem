#include "ui/StatusChangeModal.h"
#include "notui/Label.h"
#include "notui/HBox.h"
#include "notui/Spacer.h"
#include "notui/Button.h"

using namespace notui;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
StatusChangeModal::StatusChangeModal(std::string title, const std::string& action_details,
                                     const std::function<void(bool, std::string)>& callback)
    : Modal(std::move(title), 50, 10) {

    auto details_lbl = std::make_shared<Label>(action_details, Size{2, 46}, true);
    details_lbl->style.fg({200, 200, 200});
    add_child(details_lbl);

    add_child(std::make_shared<Label>("Add Note (optional):", Size{1, 20}));
    notes_in = std::make_shared<InputBox<std::string>>("Notes...", 30);
    add_child(notes_in);

    auto btn_row = std::make_shared<HBox>();
    btn_row->fixed_height = 1;
    btn_row->main_axis_alignment = MainAxisAlignment::Center;

    auto confirm_btn = std::make_shared<Button>(
        "Confirm", [this, callback]() { callback(true, notes_in->get_value()); });
    confirm_btn->style.bg({40, 120, 60}).fg({255, 255, 255});
    btn_row->add_child(confirm_btn);

    auto spacer = std::make_shared<Spacer>();
    spacer->fixed_width = 4;
    spacer->flex = 0;
    btn_row->add_child(spacer);

    cancel_btn = std::make_shared<Button>("Cancel", [=]() { callback(false, ""); });
    cancel_btn->style.bg({120, 40, 40}).fg({255, 255, 255});
    btn_row->add_child(cancel_btn);

    add_child(btn_row);
}
