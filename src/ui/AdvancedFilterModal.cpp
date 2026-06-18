#include "ui/AdvancedFilterModal.h"
#include "notui/ScrollArea.h"
#include "notui/Label.h"
#include "notui/HBox.h"
#include "notui/Spacer.h"
#include "notui/Button.h"
#include <algorithm>
#include <cctype>

using namespace notui;

AdvancedFilterModal::AdvancedFilterModal(
    const AdvancedFilter& current_filter,
    const std::function<void(std::optional<AdvancedFilter>)>& callback)
    : Modal("Advanced Filters", 58, 20) {

    auto scroll = std::make_shared<ScrollArea>();
    scroll->flex = 1;
    scroll->style.bg({30, 30, 35});

    auto int_validator = [](const std::string& str) {
        return str.empty() || std::all_of(str.begin(), str.end(), [](unsigned char character) {
                   return std::isdigit(character) != 0;
               });
    };

    auto double_validator = [](const std::string& str) {
        if (str.empty()) {
            return true;
        }
        try {
            std::stod(str);
            return true;
        } catch (...) {
            return false;
        }
    };

    auto add_filter_row =
        [&](const std::string& field_name,
            std::shared_ptr<InputBox<std::string>>& // NOLINT(bugprone-easily-swappable-parameters)
                min_box,                            // NOLINT(bugprone-easily-swappable-parameters)
            std::shared_ptr<InputBox<std::string>>& // NOLINT(bugprone-easily-swappable-parameters)
                max_box,                            // NOLINT(bugprone-easily-swappable-parameters)
            std::shared_ptr<InputBox<std::string>>& // NOLINT(bugprone-easily-swappable-parameters)
                eq_box,                             // NOLINT(bugprone-easily-swappable-parameters)
            const FilterRequirement& req,
            const std::function<bool(const std::string&)>& validator = nullptr) {
            auto row = std::make_shared<HBox>();
            row->fixed_height = 1;
            row->flex = 0;

            auto lbl = std::make_shared<Label>(field_name + ":", Size{1, 14});
            lbl->style.fg({200, 200, 200});
            row->add_child(lbl);

            min_box = std::make_shared<InputBox<std::string>>("Min", 10, validator);
            if (req.min_val.has_value()) {
                min_box->set_value(*req.min_val);
            }
            row->add_child(min_box);

            auto sp_inner1 = std::make_shared<Spacer>();
            sp_inner1->fixed_width = 1;
            sp_inner1->flex = 0;
            row->add_child(sp_inner1);

            max_box = std::make_shared<InputBox<std::string>>("Max", 10, validator);
            if (req.max_val.has_value()) {
                max_box->set_value(*req.max_val);
            }
            row->add_child(max_box);

            auto sp_inner2 = std::make_shared<Spacer>();
            sp_inner2->fixed_width = 1;
            sp_inner2->flex = 0;
            row->add_child(sp_inner2);

            eq_box = std::make_shared<InputBox<std::string>>("Equals", 10, validator);
            if (req.equals_val.has_value()) {
                eq_box->set_value(*req.equals_val);
            }
            row->add_child(eq_box);

            scroll->add_child(row);

            auto row_sp = std::make_shared<Spacer>();
            row_sp->fixed_height = 1;
            row_sp->flex = 0;
            scroll->add_child(row_sp);
        };

    auto add_text_filter_row = [&](const std::string& field_name,
                                   std::shared_ptr<InputBox<std::string>>& eq_box,
                                   const FilterRequirement& req) {
        auto row = std::make_shared<HBox>();
        row->fixed_height = 1;
        row->flex = 0;

        auto lbl = std::make_shared<Label>(field_name + ":", Size{1, 14});
        lbl->style.fg({200, 200, 200});
        row->add_child(lbl);

        eq_box = std::make_shared<InputBox<std::string>>("Equals", 32);
        if (req.equals_val.has_value()) {
            eq_box->set_value(*req.equals_val);
        }
        row->add_child(eq_box);

        scroll->add_child(row);

        auto row_sp = std::make_shared<Spacer>();
        row_sp->fixed_height = 1;
        row_sp->flex = 0;
        scroll->add_child(row_sp);
    };

    add_filter_row("Vehicle ID", id_min, id_max, id_eq, current_filter.id, int_validator);
    add_text_filter_row("Brand Name", brand_eq, current_filter.brand);
    add_text_filter_row("Model/Type", model_eq, current_filter.model);
    add_text_filter_row("Type", type_eq, current_filter.type);
    add_text_filter_row("Status", status_eq, current_filter.status);
    add_filter_row("PLN/hr Rate", price_min, price_max, price_eq, current_filter.price,
                   double_validator);
    add_filter_row("Seats (Car)", seats_min, seats_max, seats_eq, current_filter.seats,
                   int_validator);
    add_filter_row("Payload (kg)", payload_min, payload_max, payload_eq, current_filter.payload,
                   int_validator);
    add_text_filter_row("Rental Code", rental_code_eq, current_filter.rental_code);

    add_child(scroll);

    auto spacer_sp = std::make_shared<Spacer>();
    spacer_sp->fixed_height = 1;
    spacer_sp->flex = 0;
    add_child(spacer_sp);

    auto btn_row = std::make_shared<HBox>();
    btn_row->fixed_height = 1;
    btn_row->main_axis_alignment = MainAxisAlignment::Center;

    auto apply_btn = std::make_shared<Button>("Apply", [this, callback]() {
        AdvancedFilter filter;
        auto get_req = [](const auto& min_b, const auto& max_b,
                          const auto& eq_b) -> FilterRequirement {
            FilterRequirement req_filter;
            std::string min_v = min_b->get_value();
            std::string max_v = max_b->get_value();
            std::string eq_v = eq_b->get_value();
            if (!min_v.empty()) {
                req_filter.min_val = min_v;
            }
            if (!max_v.empty()) {
                req_filter.max_val = max_v;
            }
            if (!eq_v.empty()) {
                req_filter.equals_val = eq_v;
            }
            return req_filter;
        };
        auto get_text_req = [](const auto& eq_b) -> FilterRequirement {
            FilterRequirement req_filter;
            std::string eq_v = eq_b->get_value();
            if (!eq_v.empty()) {
                req_filter.equals_val = eq_v;
            }
            return req_filter;
        };
        filter.id = get_req(id_min, id_max, id_eq);
        filter.brand = get_text_req(brand_eq);
        filter.model = get_text_req(model_eq);
        filter.type = get_text_req(type_eq);
        filter.status = get_text_req(status_eq);
        filter.price = get_req(price_min, price_max, price_eq);
        filter.seats = get_req(seats_min, seats_max, seats_eq);
        filter.payload = get_req(payload_min, payload_max, payload_eq);
        filter.rental_code = get_text_req(rental_code_eq);

        callback(filter);
    });
    apply_btn->style.bg({40, 120, 60}).fg({255, 255, 255});
    btn_row->add_child(apply_btn);

    auto spacer = std::make_shared<Spacer>();
    spacer->fixed_width = 4;
    spacer->flex = 0;
    btn_row->add_child(spacer);

    cancel_btn = std::make_shared<Button>("Cancel", [callback]() { callback(std::nullopt); });
    cancel_btn->style.bg({120, 40, 40}).fg({255, 255, 255});
    btn_row->add_child(cancel_btn);

    add_child(btn_row);
}
