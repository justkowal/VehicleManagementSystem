#include "UI.h"
#include "AppPaths.h"
#include "FileStorage.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

#include <cmath>
#include <regex>

using namespace notui;

namespace {
auto uiFormatMoney(int amount_grosz) -> std::string {
    int zlotys = amount_grosz / 100;
    int groszy = std::abs(amount_grosz % 100);
    std::ostringstream oss;
    oss << zlotys << "." << std::setw(2) << std::setfill('0') << groszy;
    return oss.str();
}

struct LogRenderInfo {
    ColorRGB color;
    std::string details;
};

auto process_log_entry(RecordType type, const std::string& raw_details) -> LogRenderInfo {
    ColorRGB color{120, 120, 120}; // default gray
    switch (type) {
    case RecordType::Checkout:
        color = {220, 160, 20}; // orange
        break;
    case RecordType::Returned:
        color = {40, 180, 80}; // green
        break;
    case RecordType::Maintenance:
        color = {220, 50, 50}; // red
        break;
    case RecordType::Note:
        color = {80, 150, 255}; // blue
        break;
    }

    std::string details = raw_details;
    static const std::regex status_pattern(R"(^\[([^\]]+)\]\s*)");
    std::smatch match;
    if (std::regex_search(raw_details, match, status_pattern)) {
        std::string status_label = match[1].str();
        details = match.suffix().str();

        std::string lower_label = status_label;
        std::transform(
            lower_label.begin(), lower_label.end(), lower_label.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

        if (lower_label == "checkout" || lower_label == "rent" || lower_label == "rented") {
            color = {220, 160, 20}; // orange
        } else if (lower_label == "returned" || lower_label == "return" || lower_label == "retn" ||
                   lower_label == "available" || lower_label == "avail") {
            color = {40, 180, 80}; // green
        } else if (lower_label == "maintenance" || lower_label == "service" ||
                   lower_label == "mntc" || lower_label == "maint") {
            color = {220, 50, 50}; // red
        } else if (lower_label == "note") {
            color = {80, 150, 255}; // blue
        } else {
            color = {120, 120, 120}; // unknown label -> gray
        }
    }

    return {color, details};
}
} // namespace

LoadingOverlay::LoadingOverlay(std::string message) {
    fixed_height = 5;
    fixed_width = 40;
    is_overlay = true;
    style.bg({35, 35, 45}).fg({255, 255, 255}).frame(true, true, " Processing ");

    auto spacer = std::make_shared<Spacer>();
    spacer->fixed_height = 1;
    spacer->flex = 0;
    add_child(spacer);

    auto msg_lbl = std::make_shared<Label>(std::move(message), Size{1, 36}, true);
    msg_lbl->style.fg({220, 160, 20}).attr(NCSTYLE_BOLD);
    add_child(msg_lbl);

    auto hint_lbl = std::make_shared<Label>("Working in background...", Size{1, 36}, true);
    hint_lbl->style.fg({120, 120, 120});
    add_child(hint_lbl);
}

auto LoadingOverlay::layout(struct ncplane* parent_plane, Point pos, Size size) -> void {
    abs_y = pos.y;
    abs_x = pos.x;
    height = size.height;
    width = size.width;

    if (backdrop_plane_ != nullptr) {
        ncplane_destroy(backdrop_plane_);
        backdrop_plane_ = nullptr;
    }

    if (parent_plane == nullptr || size.height <= 0 || size.width <= 0) {
        return;
    }

    struct ncplane_options b_opts = {.y = 0,
                                     .x = 0,
                                     .rows = static_cast<unsigned>(size.height),
                                     .cols = static_cast<unsigned>(size.width),
                                     .userptr = this,
                                     .name = "loading_backdrop",
                                     .resizecb = nullptr,
                                     .flags = 0,
                                     .margin_b = 0,
                                     .margin_r = 0};
    backdrop_plane_ = ncplane_create(parent_plane, &b_opts);
    if (backdrop_plane_ != nullptr) {
        ncplane_move_top(backdrop_plane_);
        uint64_t channels = 0;
        ncchannels_set_bg_rgb8(&channels, 10, 10, 15);
        ncchannels_set_bg_alpha(&channels, NCALPHA_BLEND);
        ncchannels_set_fg_alpha(&channels, NCALPHA_TRANSPARENT);
        ncplane_set_base(backdrop_plane_, "", 0, channels);
        ncplane_erase(backdrop_plane_);
    }

    int center_y = (size.height - fixed_height) / 2;
    int center_x = (size.width - fixed_width) / 2;
    VBox::layout(backdrop_plane_, Point{center_y, center_x}, Size{fixed_height, fixed_width});
}

auto LoadingOverlay::destroy_planes() -> void {
    VBox::destroy_planes();
    if (backdrop_plane_ != nullptr) {
        ncplane_destroy(backdrop_plane_);
        backdrop_plane_ = nullptr;
    }
}

auto LoadingOverlay::raise_to_top() -> void {
    if (backdrop_plane_ != nullptr) {
        ncplane_move_top(backdrop_plane_);
    }
    VBox::raise_to_top();
}

LogsModal::LogsModal(std::string title, const std::vector<Record>& records,
                     std::function<void()> on_close)
    : on_close_(std::move(on_close)) {
    fixed_height = 16;
    fixed_width = 70;
    is_overlay = true;
    style.bg({30, 30, 35}).fg({255, 255, 255}).frame(true, true);

    title_label_ = std::make_shared<Label>(std::move(title), Size{1, 60}, true);
    title_label_->style.fg({80, 150, 255}).attr(NCSTYLE_BOLD);
    add_child(title_label_);

    auto spacer = std::make_shared<Spacer>();
    spacer->fixed_height = 1;
    spacer->flex = 0;
    add_child(spacer);

    scroll_area_ = std::make_shared<ScrollArea>();
    scroll_area_->flex = 1;
    scroll_area_->style.bg({20, 20, 25}).frame(true, true, " History Logs ").pad({0, 1, 0, 1});

    if (records.empty()) {
        auto lbl = std::make_shared<Label>("No transaction logs found for this vehicle.",
                                           Size{1, 55}, true);
        lbl->style.fg({150, 150, 150});
        scroll_area_->add_child(lbl);
    } else {
        for (size_t i = 0; i < records.size(); ++i) {
            const auto& rec = records[i];

            if (i > 0) {
                auto line_row = std::make_shared<HBox>();
                line_row->fixed_height = 1;
                line_row->flex = 0;

                auto line_lbl = std::make_shared<Label>("│", Size{1, 1});
                line_lbl->style.fg({80, 80, 80});
                line_lbl->style.pad({0, 0, 0, 0});
                line_row->add_child(line_lbl);

                scroll_area_->add_child(line_row);
            }

            auto row = std::make_shared<HBox>();
            row->fixed_height = 1;
            row->flex = 0;

            auto log_info = process_log_entry(rec.type, rec.details);

            auto circle_lbl = std::make_shared<Label>("●", Size{1, 1});
            circle_lbl->style.fg(log_info.color);
            circle_lbl->style.pad({0, 0, 0, 0});
            row->add_child(circle_lbl);

            auto sp1 = std::make_shared<Spacer>();
            sp1->fixed_width = 1;
            sp1->flex = 0;
            row->add_child(sp1);

            std::time_t raw_time = std::chrono::system_clock::to_time_t(rec.timestamp);
            std::tm tm_struct{};
            gmtime_r(&raw_time, &tm_struct);
            std::array<char, 20> buffer{};
            std::strftime(buffer.data(), buffer.size(), "%m-%d %H:%M", &tm_struct);
            std::string time_str(buffer.data());

            auto time_lbl = std::make_shared<Label>("[" + time_str + "]", Size{1, 13});
            time_lbl->style.fg({120, 120, 120});
            time_lbl->style.pad({0, 0, 0, 0});
            row->add_child(time_lbl);

            auto sp2 = std::make_shared<Spacer>();
            sp2->fixed_width = 1;
            sp2->flex = 0;
            row->add_child(sp2);

            auto details_lbl = std::make_shared<Label>(log_info.details, Size{1, 0});
            details_lbl->flex = 1;
            details_lbl->style.fg({200, 200, 200});
            row->add_child(details_lbl);

            scroll_area_->add_child(row);
        }
    }
    add_child(scroll_area_);

    auto sp2 = std::make_shared<Spacer>();
    sp2->fixed_height = 1;
    sp2->flex = 0;
    add_child(sp2);

    close_btn_ = std::make_shared<Button>("OK", [this]() {
        if (this->on_close_) {
            this->on_close_();
        }
    });
    close_btn_->style.bg({80, 150, 255}).fg({0, 0, 0}).attr(NCSTYLE_BOLD);
    add_child(close_btn_);
}

auto LogsModal::layout(struct ncplane* parent_plane, Point pos, Size size) -> void {
    abs_y = pos.y;
    abs_x = pos.x;
    height = size.height;
    width = size.width;

    if (backdrop_plane_ != nullptr) {
        ncplane_destroy(backdrop_plane_);
        backdrop_plane_ = nullptr;
    }

    if (parent_plane == nullptr || size.height <= 0 || size.width <= 0) {
        return;
    }

    struct ncplane_options b_opts = {.y = 0,
                                     .x = 0,
                                     .rows = static_cast<unsigned>(size.height),
                                     .cols = static_cast<unsigned>(size.width),
                                     .userptr = this,
                                     .name = "logs_backdrop",
                                     .resizecb = nullptr,
                                     .flags = 0,
                                     .margin_b = 0,
                                     .margin_r = 0};
    backdrop_plane_ = ncplane_create(parent_plane, &b_opts);
    if (backdrop_plane_ != nullptr) {
        ncplane_move_top(backdrop_plane_);
        uint64_t channels = 0;
        ncchannels_set_bg_rgb8(&channels, 10, 10, 15);
        ncchannels_set_bg_alpha(&channels, NCALPHA_BLEND);
        ncchannels_set_fg_alpha(&channels, NCALPHA_TRANSPARENT);
        ncplane_set_base(backdrop_plane_, "", 0, channels);
        ncplane_erase(backdrop_plane_);
    }

    int center_y = (size.height - fixed_height) / 2;
    int center_x = (size.width - fixed_width) / 2;
    VBox::layout(backdrop_plane_, Point{center_y, center_x}, Size{fixed_height, fixed_width});
}

auto LogsModal::destroy_planes() -> void {
    VBox::destroy_planes();
    if (backdrop_plane_ != nullptr) {
        ncplane_destroy(backdrop_plane_);
        backdrop_plane_ = nullptr;
    }
}

auto LogsModal::raise_to_top() -> void {
    if (backdrop_plane_ != nullptr) {
        ncplane_move_top(backdrop_plane_);
    }
    VBox::raise_to_top();
}

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

RentVehicleModal::RentVehicleModal(
    const Vehicle& vehicle,
    const std::function<void(bool, std::string, std::string, std::string)>& callback)
    : Modal("Confirm Rental", 50, 13) {

    std::string details = "Renting: " + vehicle.getBrand() + " " + vehicle.getModelOrType() +
                          "\nRate: " + uiFormatMoney(vehicle.getPricePerHour()) + " PLN/h";
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

UI::UI(FleetManager& manager) : manager_(manager) {
    root_ = std::make_shared<VBox>();
    root_->style.bg({15, 15, 20}).pad({1, 1, 1, 1});

    compositor_ = std::make_shared<Compositor>(root_);
    compositor_->set_idle_callback([this]() { this->process_result_queue(); });

    build_ui();

    compositor_->set_global_shortcut_handler([this](const ncinput& nc_input) -> bool {
        return this->handle_global_shortcuts(nc_input);
    });

    trigger_load_fleet();
}

auto UI::handle_global_shortcuts(const ncinput& nc_input) -> bool {
    if (nc_input.evtype == NCTYPE_RELEASE) {
        return false;
    }

    const auto key = nc_input.id;
    const bool ctrl_pressed = nc_input.ctrl || ((nc_input.modifiers & NCKEY_MOD_CTRL) != 0);
    bool is_q = (ctrl_pressed && (key == 'q' || key == 'Q')) || (key == 17);
    bool is_r = (ctrl_pressed && (key == 'r' || key == 'R')) || (key == 18);
    bool is_n = (ctrl_pressed && (key == 'n' || key == 'N')) || (key == 14);
    bool is_f = (ctrl_pressed && (key == 'f' || key == 'F')) || (key == 6);

    if (is_q) {
        show_confirm_quit();
        return true;
    }

    if (notui::FocusManager::get_active_overlay(root_.get()) != nullptr) {
        return false;
    }

    if (is_r) {
        show_quick_return_modal();
        return true;
    }
    if (is_n) {
        auto modal_holder = std::make_shared<std::shared_ptr<RegisterVehicleModal>>();
        *modal_holder = std::make_shared<RegisterVehicleModal>(
            [this, modal_holder](const std::optional<Vehicle>& vehicle_opt) {
                this->root_->remove_child(*modal_holder);
                this->compositor_->trigger_layout();
                if (vehicle_opt.has_value()) {
                    this->trigger_add_vehicle(*vehicle_opt);
                }
            });
        this->root_->add_child(*modal_holder);
        this->compositor_->trigger_layout();
        return true;
    }
    if (is_f) {
        if (search_input_) {
            compositor_->get_focus_manager().set_focus(search_input_.get());
            compositor_->trigger_layout();
        }
        return true;
    }

    return false;
}

auto UI::run() -> void {
    compositor_->run();
}

auto UI::build_ui() -> void {
    auto header =
        std::make_shared<Label>("Fake rental - Fleet Management System", Size{3, 0}, true);
    header->style.bg({35, 75, 180}).fg({255, 255, 255}).frame(true, true).attr(NCSTYLE_BOLD);
    header->flex = 0;
    root_->add_child(header);

    auto sp_h = std::make_shared<Spacer>();
    sp_h->fixed_height = 1;
    sp_h->flex = 0;
    root_->add_child(sp_h);

    auto stats_row = std::make_shared<HBox>();
    stats_row->fixed_height = 3;
    stats_row->style.bg({30, 30, 35}).frame(true, true, " Fleet Overview ").pad({0, 1, 0, 1});
    stats_row->flex = 0;

    total_stat_ = std::make_shared<Label>("Total: 0", Size{1, 15});
    total_stat_->style.fg({255, 255, 255});
    stats_row->add_child(total_stat_);

    avail_stat_ = std::make_shared<Label>("Available: 0", Size{1, 18});
    avail_stat_->style.fg({40, 180, 80});
    stats_row->add_child(avail_stat_);

    rent_stat_ = std::make_shared<Label>("Rented: 0", Size{1, 15});
    rent_stat_->style.fg({220, 160, 20});
    stats_row->add_child(rent_stat_);

    maint_stat_ = std::make_shared<Label>("Maintenance: 0", Size{1, 20});
    maint_stat_->style.fg({220, 50, 50});
    stats_row->add_child(maint_stat_);

    root_->add_child(stats_row);

    auto sp_s = std::make_shared<Spacer>();
    sp_s->fixed_height = 1;
    sp_s->flex = 0;
    root_->add_child(sp_s);

    auto top_bar = std::make_shared<HBox>();
    top_bar->fixed_height = 3;
    top_bar->style.bg({25, 25, 30}).frame(true, true, " Filters ").pad({0, 1, 0, 1});
    top_bar->flex = 0;

    top_bar->add_child(std::make_shared<Label>("Search:", Size{1, 8}));
    search_input_ = std::make_shared<InputBox<std::string>>(
        "Search fleet (brand, model, ID, status, rate, specs)...", 60);
    search_input_->on("change", [this](const Event&) { this->refresh_fleet_list(); });
    top_bar->add_child(search_input_);

    auto sp_f1 = std::make_shared<Spacer>();
    sp_f1->fixed_width = 2;
    sp_f1->flex = 0;
    top_bar->add_child(sp_f1);

    type_filter_ = std::make_shared<Dropdown>(
        std::vector<std::string>{"All Types", "Cars", "Bikes", "Trucks"});
    type_filter_->on("change", [this](const Event&) { this->refresh_fleet_list(); });
    top_bar->add_child(type_filter_);

    auto sp_f2 = std::make_shared<Spacer>();
    sp_f2->fixed_width = 2;
    sp_f2->flex = 0;
    top_bar->add_child(sp_f2);

    status_filter_ = std::make_shared<Dropdown>(
        std::vector<std::string>{"All Statuses", "Available", "Rented", "Maintenance"});
    status_filter_->on("change", [this](const Event&) { this->refresh_fleet_list(); });
    top_bar->add_child(status_filter_);

    auto sp_f3 = std::make_shared<Spacer>();
    sp_f3->fixed_width = 2;
    sp_f3->flex = 0;
    top_bar->add_child(sp_f3);

    secondary_info_filter_ = std::make_shared<Dropdown>(
        std::vector<std::string>{"Show: Status", "Show: Specs", "Show: Rental Code", "Show: ID"});
    secondary_info_filter_->on("change", [this](const Event&) { this->refresh_fleet_list(); });
    top_bar->add_child(secondary_info_filter_);

    auto sp_f4 = std::make_shared<Spacer>();
    sp_f4->fixed_width = 2;
    sp_f4->flex = 0;
    top_bar->add_child(sp_f4);

    advanced_filter_btn_ = std::make_shared<Button>("Advanced filters", [this]() {
        if (advanced_filter_.has_value() && advanced_filter_->active()) {
            advanced_filter_ = std::nullopt;
            advanced_filter_btn_->label = "Advanced filters";
            this->refresh_fleet_list();
        } else {
            this->show_advanced_filter_modal();
        }
    });
    advanced_filter_btn_->style.bg({80, 150, 255}).fg({0, 0, 0});
    top_bar->add_child(advanced_filter_btn_);

    root_->add_child(top_bar);

    auto sp_m = std::make_shared<Spacer>();
    sp_m->fixed_height = 1;
    sp_m->flex = 0;
    root_->add_child(sp_m);

    split_area_ = std::make_shared<SplitBox>(SplitBox::Orientation::Horizontal, 0.5);
    split_area_->flex = 1;
    split_area_->set_split_enabled(false);

    left_list_panel_ = std::make_shared<ScrollArea>();
    left_list_panel_->flex = 1;
    left_list_panel_->style.bg({20, 20, 25}).frame(true, true, " Vehicle List ").pad({1, 1, 1, 1});
    split_area_->add_child(left_list_panel_);

    right_detail_panel_ = std::make_shared<VBox>();
    right_detail_panel_->flex = 0;
    right_detail_panel_->fixed_width = 0;
    right_detail_panel_->style.bg({25, 25, 32})
        .frame(true, true, " Vehicle Details ")
        .pad({1, 2, 1, 2});
    split_area_->add_child(right_detail_panel_);

    root_->add_child(split_area_);

    auto sp_b = std::make_shared<Spacer>();
    sp_b->fixed_height = 1;
    sp_b->flex = 0;
    root_->add_child(sp_b);

    auto footer_bar = std::make_shared<HBox>();
    footer_bar->fixed_height = 1;
    footer_bar->flex = 0;

    auto add_vehicle_btn = std::make_shared<Button>("Register New Vehicle", [this]() {
        auto modal_holder = std::make_shared<std::shared_ptr<RegisterVehicleModal>>();
        *modal_holder = std::make_shared<RegisterVehicleModal>(
            [this, modal_holder](const std::optional<Vehicle>& vehicle_opt) {
                this->root_->remove_child(*modal_holder);
                this->compositor_->trigger_layout();
                if (vehicle_opt.has_value()) {
                    this->trigger_add_vehicle(*vehicle_opt);
                }
            });
        this->root_->add_child(*modal_holder);
        this->compositor_->trigger_layout();
    });
    add_vehicle_btn->style.bg({40, 120, 60}).fg({255, 255, 255});
    footer_bar->add_child(add_vehicle_btn);

    auto footer_spacer = std::make_shared<Spacer>();
    footer_spacer->flex = 1;
    footer_bar->add_child(footer_spacer);

    auto quick_return_btn =
        std::make_shared<Button>("Return with Code", [this]() { this->show_quick_return_modal(); });
    quick_return_btn->style.bg({220, 160, 20}).fg({0, 0, 0}).attr(NCSTYLE_BOLD);
    footer_bar->add_child(quick_return_btn);

    auto footer_spacer2 = std::make_shared<Spacer>();
    footer_spacer2->fixed_width = 3;
    footer_spacer2->flex = 0;
    footer_bar->add_child(footer_spacer2);

    auto quit_btn = std::make_shared<Button>("Exit App", [this]() { this->show_confirm_quit(); });
    quit_btn->style.bg({120, 40, 40}).fg({255, 255, 255});
    footer_bar->add_child(quit_btn);

    root_->add_child(footer_bar);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto UI::refresh_fleet_list() -> void {
    auto children_list = left_list_panel_->get_children();
    for (const auto& child : children_list) {
        left_list_panel_->remove_child(child);
    }

    std::string search_query = search_input_->get_value();
    std::transform(
        search_query.begin(), search_query.end(), search_query.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

    int type_idx = type_filter_->get_selected_index();
    int status_idx = status_filter_->get_selected_index();
    int display_idx = secondary_info_filter_->get_selected_index();

    bool has_visible_cards = false;

    auto matched_vehicles = manager_.searchFleet(
        [&](const Vehicle& veh) {
            std::string type = veh.getTypeString();
            if (type_idx == 1 && type != "Car") {
                return false;
            }
            if (type_idx == 2 && type != "Bike") {
                return false;
            }
            if (type_idx == 3 && type != "Truck") {
                return false;
            }

            VehicleStatus status = veh.getStatus();
            if (status_idx == 1 && status != VehicleStatus::Available) {
                return false;
            }
            if (status_idx == 2 && status != VehicleStatus::Rented) {
                return false;
            }
            if (status_idx == 3 && status != VehicleStatus::Maintenance) {
                return false;
            }

            if (search_query.empty()) {
                return true;
            }

            auto matches = [&](const std::string& field_val) {
                std::string val_lower = field_val;
                std::transform(val_lower.begin(), val_lower.end(), val_lower.begin(),
                               [](unsigned char character) {
                                   return static_cast<char>(std::tolower(character));
                               });
                return val_lower.find(search_query) != std::string::npos;
            };

            if (matches(std::to_string(veh.getId()))) {
                return true;
            }
            if (matches(veh.getBrand())) {
                return true;
            }
            if (matches(veh.getModelOrType())) {
                return true;
            }
            if (matches(veh.getTypeString())) {
                return true;
            }
            if (matches(veh.getStatusString())) {
                return true;
            }

            auto rental_code_opt = manager_.getRentalCodeByVehicleId(veh.getId());
            if (rental_code_opt.has_value() && matches(rental_code_opt.value())) {
                return true;
            }

            if (matches(uiFormatMoney(veh.getPricePerHour()))) {
                return true;
            }

            std::string specs_text;
            std::visit(
                [&](const auto& item) {
                    using T = std::decay_t<decltype(item)>;
                    if constexpr (std::is_same_v<T, Car>) {
                        specs_text = "Capacity: " + std::to_string(item.seats) + " seats";
                    } else if constexpr (std::is_same_v<T, Truck>) {
                        specs_text =
                            "Payload capacity: " + std::to_string(item.payload_capacity_kg) + " kg";
                    } else {
                        specs_text = "Standard road category model";
                    }
                },
                veh.getVariant());

            return matches(specs_text);
        },
        advanced_filter_);

    for (const auto& veh : matched_vehicles) {
        has_visible_cards = true;

        VehicleStatus stat = veh.getStatus();

        auto card = std::make_shared<VehicleCard>(veh.getId());
        card->fixed_height = 4;

        bool is_this_selected =
            (selected_vehicle_id_.has_value() && selected_vehicle_id_.value() == veh.getId());
        if (is_this_selected) {
            card->style.bg({45, 50, 70}).fg({255, 255, 255}).frame(true, true);
        } else {
            card->style.bg({30, 30, 38}).fg({220, 220, 220}).frame(true, true);
        }

        auto head_row = std::make_shared<HBox>();
        head_row->fixed_height = 1;
        head_row->flex = 0;
        head_row->main_axis_alignment = MainAxisAlignment::SpaceBetween;

        std::string card_title = veh.getBrand() + " " + veh.getModelOrType() +
                                 " (ID: " + std::to_string(veh.getId()) + ")";
        auto title_lbl = std::make_shared<Label>(card_title, Size{1, 30});
        title_lbl->set_highlight_query(search_query);
        title_lbl->style.fg({255, 255, 255}).attr(NCSTYLE_BOLD);
        head_row->add_child(title_lbl);

        std::string type_tag = " [" + veh.getTypeString() + "] ";
        auto type_lbl = std::make_shared<Label>(type_tag, Size{1, 10});
        type_lbl->set_highlight_query(search_query);
        type_lbl->style.fg({120, 160, 220});
        head_row->add_child(type_lbl);

        card->add_child(head_row);

        // match specs
        std::string specs_text;
        std::visit(
            [&](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, Car>) {
                    specs_text = "Capacity: " + std::to_string(item.seats) + " seats";
                } else if constexpr (std::is_same_v<T, Truck>) {
                    specs_text =
                        "Payload capacity: " + std::to_string(item.payload_capacity_kg) + " kg";
                } else {
                    specs_text = "Standard road category model";
                }
            },
            veh.getVariant());

        bool spec_matched = false;
        if (!search_query.empty()) {
            std::string specs_lower = specs_text;
            std::transform(
                specs_lower.begin(), specs_lower.end(), specs_lower.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            if (specs_lower.find(search_query) != std::string::npos) {
                spec_matched = true;
            }
        }

        // active rental code
        std::string rental_code_str = "Code: None";
        auto rental_code_opt = manager_.getRentalCodeByVehicleId(veh.getId());
        if (rental_code_opt.has_value()) {
            rental_code_str = "Code: " + rental_code_opt.value();
        }

        bool rental_code_matched = false;
        if (!search_query.empty() && rental_code_opt.has_value()) {
            std::string code_lower = rental_code_opt.value();
            std::transform(
                code_lower.begin(), code_lower.end(), code_lower.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            if (code_lower.find(search_query) != std::string::npos) {
                rental_code_matched = true;
            }
        }

        // status text
        std::string stat_str = "Status: [" + veh.getStatusString();
        if (veh.getStatus() == VehicleStatus::Rented) {
            auto hours_opt = manager_.getRentalBilledHours(veh.getId());
            if (hours_opt.has_value()) {
                stat_str += " (" + std::to_string(*hours_opt) + "h)";
            }
        }
        stat_str += "]";
        bool status_matched = false;
        if (!search_query.empty()) {
            std::string status_lower = stat_str;
            std::transform(
                status_lower.begin(), status_lower.end(), status_lower.begin(),
                [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            if (status_lower.find(search_query) != std::string::npos) {
                status_matched = true;
            }
        }

        // card body
        auto body_row = std::make_shared<HBox>();
        body_row->fixed_height = 1;
        body_row->flex = 0;
        body_row->main_axis_alignment = MainAxisAlignment::SpaceBetween;

        std::ostringstream rate_ss;
        rate_ss << "Rate: " << uiFormatMoney(veh.getPricePerHour()) << " PLN/h";
        auto rate_lbl = std::make_shared<Label>(rate_ss.str(), Size{1, 22});
        rate_lbl->set_highlight_query(search_query);
        rate_lbl->style.fg({180, 180, 180});
        body_row->add_child(rate_lbl);

        // secondary label selection
        int final_display = display_idx;
        if (!search_query.empty()) {
            bool current_matched = false;
            if ((display_idx == 0 && status_matched) || (display_idx == 1 && spec_matched) ||
                (display_idx == 2 && rental_code_matched)) {
                current_matched = true;
            }

            if (!current_matched) {
                // override matched label
                if (spec_matched) {
                    final_display = 1;
                } else if (rental_code_matched) {
                    final_display = 2;
                } else if (status_matched) {
                    final_display = 0;
                }
            }
        }

        if (final_display == 1) { // specs
            auto spec_lbl = std::make_shared<Label>(specs_text, Size{1, 24});
            spec_lbl->set_highlight_query(search_query);
            spec_lbl->style.fg({180, 180, 180});
            body_row->add_child(spec_lbl);
        } else if (final_display == 2) { // rental code
            auto rent_lbl = std::make_shared<Label>(rental_code_str, Size{1, 24});
            rent_lbl->set_highlight_query(search_query);
            rent_lbl->style.fg({180, 180, 180});
            body_row->add_child(rent_lbl);
        } else if (final_display == 3) { // id
            auto id_lbl =
                std::make_shared<Label>("ID: " + std::to_string(veh.getId()), Size{1, 20});
            id_lbl->set_highlight_query(search_query);
            id_lbl->style.fg({180, 180, 180});
            body_row->add_child(id_lbl);
        } else { // status
            auto stat_lbl = std::make_shared<Label>(stat_str, Size{1, 25});
            stat_lbl->set_highlight_query(search_query);
            if (stat == VehicleStatus::Available) {
                stat_lbl->style.fg({40, 180, 80}).attr(NCSTYLE_BOLD);
            } else if (stat == VehicleStatus::Rented) {
                stat_lbl->style.fg({220, 160, 20});
            } else {
                stat_lbl->style.fg({220, 50, 50});
            }
            body_row->add_child(stat_lbl);
        }

        card->add_child(body_row);

        card->on("focus", [this, vehicle_id = veh.getId()](const Event&) {
            this->select_vehicle(vehicle_id);
        });

        left_list_panel_->add_child(card);
    }

    if (!has_visible_cards) {
        auto empty_lbl =
            std::make_shared<Label>("No matching vehicles in fleet.", Size{1, 30}, true);
        empty_lbl->style.fg({150, 150, 150});
        left_list_panel_->add_child(empty_lbl);
    }

    // update details
    rebuild_details_panel();

    compositor_->trigger_layout();
}

auto UI::select_vehicle(uint32_t vehicle_id) -> void {
    if (selected_vehicle_id_.has_value() && selected_vehicle_id_.value() == vehicle_id) {
        return;
    }
    selected_vehicle_id_ = vehicle_id;
    is_loading_logs_ = true;
    trigger_load_logs(vehicle_id);

    // adjust split view
    split_area_->set_split_enabled(true);

    // update card highlights
    for (const auto& child : left_list_panel_->get_children()) {
        auto card = std::dynamic_pointer_cast<VehicleCard>(child);
        if (card != nullptr) {
            if (card->vehicle_id == vehicle_id) {
                card->style.bg({45, 50, 70}).fg({255, 255, 255});
            } else {
                card->style.bg({30, 30, 38}).fg({220, 220, 220});
            }
        }
    }

    // rebuild details
    rebuild_details_panel();

    // trigger layout
    compositor_->trigger_layout();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto UI::rebuild_details_panel() -> void {
    auto right_children = right_detail_panel_->get_children();
    for (const auto& child : right_children) {
        right_detail_panel_->remove_child(child);
    }

    if (!selected_vehicle_id_.has_value()) {
        return;
    }

    auto sel_it =
        std::find_if(current_fleet_.begin(), current_fleet_.end(), [this](const Vehicle& vehicle) {
            return vehicle.getId() == this->selected_vehicle_id_.value();
        });

    if (sel_it != current_fleet_.end()) {
        const auto& veh = *sel_it;

        std::string title_text = veh.getBrand() + " " + veh.getModelOrType();
        auto d_title = std::make_shared<Label>(title_text, Size{1, 35});
        d_title->style.fg({255, 255, 255}).attr(NCSTYLE_BOLD);
        right_detail_panel_->add_child(d_title);

        std::string id_text = "Unique Vehicle ID: " + std::to_string(veh.getId()) +
                              "  |  Type: " + veh.getTypeString();
        auto d_id = std::make_shared<Label>(id_text, Size{1, 35});
        d_id->style.fg({150, 150, 150});
        right_detail_panel_->add_child(d_id);

        // variant details
        std::string specs_text;
        std::visit(
            [&](const auto& item) {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, Car>) {
                    specs_text = "Capacity: " + std::to_string(item.seats) + " seats";
                } else if constexpr (std::is_same_v<T, Truck>) {
                    specs_text =
                        "Payload capacity: " + std::to_string(item.payload_capacity_kg) + " kg";
                } else {
                    specs_text = "Standard road category model";
                }
            },
            veh.getVariant());

        auto d_spec = std::make_shared<Label>(specs_text, Size{1, 35});
        d_spec->style.fg({150, 150, 150});
        right_detail_panel_->add_child(d_spec);

        std::ostringstream rate_ss;
        rate_ss << "Rental Rate: " << uiFormatMoney(veh.getPricePerHour()) << " PLN per hour";
        auto d_rate = std::make_shared<Label>(rate_ss.str(), Size{1, 35});
        d_rate->style.fg({180, 180, 180});
        right_detail_panel_->add_child(d_rate);

        // status label
        auto status_row = std::make_shared<HBox>();
        status_row->fixed_height = 1;
        status_row->flex = 0;
        status_row->add_child(std::make_shared<Label>("Current Status: ", Size{1, 16}));
        auto stat_val_lbl = std::make_shared<Label>(veh.getStatusString(), Size{1, 15});
        if (veh.getStatus() == VehicleStatus::Available) {
            stat_val_lbl->style.fg({40, 180, 80}).attr(NCSTYLE_BOLD);
        } else if (veh.getStatus() == VehicleStatus::Rented) {
            stat_val_lbl->style.fg({220, 160, 20}).attr(NCSTYLE_BOLD);
        } else {
            stat_val_lbl->style.fg({220, 50, 50}).attr(NCSTYLE_BOLD);
        }
        status_row->add_child(stat_val_lbl);
        right_detail_panel_->add_child(status_row);

        auto sp_dt = std::make_shared<Spacer>();
        sp_dt->fixed_height = 1;
        sp_dt->flex = 0;
        right_detail_panel_->add_child(sp_dt);

        // history header
        auto logs_header = std::make_shared<Label>("--- Transaction History Logs ---", Size{1, 35});
        logs_header->style.fg({100, 160, 255}).attr(NCSTYLE_BOLD);
        right_detail_panel_->add_child(logs_header);

        // log entries
        auto log_scroll = std::make_shared<ScrollArea>();
        log_scroll->flex = 1;
        log_scroll->style.bg({20, 20, 22}).pad({0, 1, 0, 1});

        if (is_loading_logs_) {
            auto load_lbl = std::make_shared<Label>("Loading records...", Size{1, 30});
            load_lbl->style.fg({120, 120, 120});
            log_scroll->add_child(load_lbl);
        } else if (selected_vehicle_logs_.empty()) {
            auto no_lbl =
                std::make_shared<Label>("No history found for this vehicle.", Size{1, 30});
            no_lbl->style.fg({120, 120, 120});
            log_scroll->add_child(no_lbl);
        } else {
            for (size_t i = 0; i < selected_vehicle_logs_.size(); ++i) {
                const auto& log = selected_vehicle_logs_[i];

                if (i > 0) {
                    auto line_row = std::make_shared<HBox>();
                    line_row->fixed_height = 1;
                    line_row->flex = 0;

                    auto line_lbl = std::make_shared<Label>("│", Size{1, 1});
                    line_lbl->style.fg({80, 80, 80});
                    line_lbl->style.pad({0, 0, 0, 0});
                    line_row->add_child(line_lbl);

                    log_scroll->add_child(line_row);
                }

                auto row = std::make_shared<HBox>();
                row->fixed_height = 1;
                row->flex = 0;

                auto log_info = process_log_entry(log.type, log.details);

                auto circle_lbl = std::make_shared<Label>("●", Size{1, 1});
                circle_lbl->style.fg(log_info.color);
                circle_lbl->style.pad({0, 0, 0, 0});
                row->add_child(circle_lbl);

                auto sp1 = std::make_shared<Spacer>();
                sp1->fixed_width = 1;
                sp1->flex = 0;
                row->add_child(sp1);

                std::time_t raw_time = std::chrono::system_clock::to_time_t(log.timestamp);
                std::tm tm_struct{};
                gmtime_r(&raw_time, &tm_struct);
                std::array<char, 20> buffer{};
                std::strftime(buffer.data(), buffer.size(), "%m-%d %H:%M", &tm_struct);
                std::string time_str(buffer.data());

                auto time_lbl = std::make_shared<Label>("[" + time_str + "]", Size{1, 13});
                time_lbl->style.fg({120, 120, 120});
                time_lbl->style.pad({0, 0, 0, 0});
                row->add_child(time_lbl);

                auto sp2 = std::make_shared<Spacer>();
                sp2->fixed_width = 1;
                sp2->flex = 0;
                row->add_child(sp2);

                auto details_lbl = std::make_shared<Label>(log_info.details, Size{1, 0});
                details_lbl->flex = 1;
                details_lbl->style.fg({200, 200, 200});
                row->add_child(details_lbl);

                log_scroll->add_child(row);
            }
        }
        right_detail_panel_->add_child(log_scroll);

        auto sp_act = std::make_shared<Spacer>();
        sp_act->fixed_height = 1;
        sp_act->flex = 0;
        right_detail_panel_->add_child(sp_act);

        // context action row
        auto action_row = std::make_shared<HBox>();
        action_row->fixed_height = 1;
        action_row->flex = 0;
        action_row->main_axis_alignment = MainAxisAlignment::SpaceBetween;

        if (veh.getStatus() == VehicleStatus::Available) {
            auto rent_btn = std::make_shared<Button>("Rent Vehicle", [this, veh]() {
                auto modal_holder = std::make_shared<std::shared_ptr<RentVehicleModal>>();
                *modal_holder = std::make_shared<RentVehicleModal>(
                    veh, [this, modal_holder, veh](bool confirmed, const std::string& name,
                                                   const std::string& surname,
                                                   const std::string& id_card) {
                        this->root_->remove_child(*modal_holder);
                        this->compositor_->trigger_layout();
                        if (confirmed) {
                            this->trigger_rent(veh.getId(), name, surname, id_card);
                        }
                    });
                this->root_->add_child(*modal_holder);
                this->compositor_->trigger_layout();
            });
            rent_btn->style.bg({40, 120, 60}).fg({255, 255, 255}).attr(NCSTYLE_BOLD);
            action_row->add_child(rent_btn);

            auto service_btn = std::make_shared<Button>("Service Check", [this, veh]() {
                auto modal_holder = std::make_shared<std::shared_ptr<StatusChangeModal>>();
                std::string details = "Sending vehicle " + veh.getBrand() + " " +
                                      veh.getModelOrType() +
                                      " (ID: " + std::to_string(veh.getId()) + ") to maintenance.";
                *modal_holder = std::make_shared<StatusChangeModal>(
                    "Confirm Service Check", details,
                    [this, modal_holder, veh, details](bool confirmed, const std::string& notes) {
                        this->root_->remove_child(*modal_holder);
                        this->compositor_->trigger_layout();
                        if (confirmed) {
                            this->trigger_update_status(veh.getId(), VehicleStatus::Maintenance,
                                                        notes);
                        }
                    });
                this->root_->add_child(*modal_holder);
                this->compositor_->trigger_layout();
            });
            service_btn->style.bg({120, 40, 40}).fg({255, 255, 255});
            action_row->add_child(service_btn);
        } else if (veh.getStatus() == VehicleStatus::Rented) {
            auto return_btn = std::make_shared<Button>("Process Return", [this, veh]() {
                auto modal_holder = std::make_shared<std::shared_ptr<StatusChangeModal>>();
                std::string details = "Processing return for vehicle " + veh.getBrand() + " " +
                                      veh.getModelOrType() +
                                      " (ID: " + std::to_string(veh.getId()) + ").";
                *modal_holder = std::make_shared<StatusChangeModal>(
                    "Confirm Return", details,
                    [this, modal_holder, veh, details](bool confirmed, const std::string& notes) {
                        this->root_->remove_child(*modal_holder);
                        this->compositor_->trigger_layout();
                        if (confirmed) {
                            this->trigger_return(veh.getId(), notes);
                        }
                    });
                this->root_->add_child(*modal_holder);
                this->compositor_->trigger_layout();
            });
            return_btn->style.bg({220, 160, 20}).fg({0, 0, 0}).attr(NCSTYLE_BOLD);
            action_row->add_child(return_btn);
        } else if (veh.getStatus() == VehicleStatus::Maintenance) {
            auto fix_btn = std::make_shared<Button>("Mark Fixed", [this, veh]() {
                auto modal_holder = std::make_shared<std::shared_ptr<StatusChangeModal>>();
                std::string details =
                    "Marking vehicle " + veh.getBrand() + " " + veh.getModelOrType() +
                    " (ID: " + std::to_string(veh.getId()) + ") as fixed and available.";
                *modal_holder = std::make_shared<StatusChangeModal>(
                    "Confirm Mark Fixed", details,
                    [this, modal_holder, veh, details](bool confirmed, const std::string& notes) {
                        this->root_->remove_child(*modal_holder);
                        this->compositor_->trigger_layout();
                        if (confirmed) {
                            this->trigger_update_status(veh.getId(), VehicleStatus::Available,
                                                        notes);
                        }
                    });
                this->root_->add_child(*modal_holder);
                this->compositor_->trigger_layout();
            });
            fix_btn->style.bg({40, 120, 60}).fg({255, 255, 255}).attr(NCSTYLE_BOLD);
            action_row->add_child(fix_btn);
        }

        auto decommission_btn = std::make_shared<Button>("Decommission", [this, veh]() {
            if (veh.getStatus() == VehicleStatus::Rented) {
                this->show_alert("Decommission Blocked",
                                 "Vehicle is currently rented and cannot be decommissioned. Please "
                                 "return it first.");
                return;
            }

            auto modal_holder = std::make_shared<std::shared_ptr<Modal>>();
            std::string msg = "Are you sure you want to permanently decommission " +
                              veh.getBrand() + " " + veh.getModelOrType() +
                              " (ID: " + std::to_string(veh.getId()) +
                              ")?\n"
                              "This will delete all transaction records and cannot be undone.";
            *modal_holder = std::make_shared<Modal>("Confirm Decommission", msg,
                                                    [this, modal_holder, veh](bool confirmed) {
                                                        this->root_->remove_child(*modal_holder);
                                                        this->compositor_->trigger_layout();
                                                        if (confirmed) {
                                                            this->trigger_decommission(veh.getId());
                                                        }
                                                    });
            this->root_->add_child(*modal_holder);
            this->compositor_->trigger_layout();
        });
        decommission_btn->style.bg({220, 50, 50}).fg({255, 255, 255}).attr(NCSTYLE_BOLD);
        action_row->add_child(decommission_btn);

        auto close_panel_btn = std::make_shared<Button>("Close Details", [this]() {
            this->selected_vehicle_id_ = std::nullopt;
            this->selected_vehicle_logs_.clear();

            this->split_area_->set_split_enabled(false);

            this->refresh_fleet_list();
        });
        close_panel_btn->style.bg({60, 60, 60}).fg({255, 255, 255});
        action_row->add_child(close_panel_btn);

        right_detail_panel_->add_child(action_row);
    }
}

auto UI::update_stats() -> void {
    size_t total = current_fleet_.size();
    size_t avail = 0;
    size_t rented = 0;
    size_t maint = 0;

    for (const auto& vehicle : current_fleet_) {
        if (vehicle.getStatus() == VehicleStatus::Available) {
            avail++;
        } else if (vehicle.getStatus() == VehicleStatus::Rented) {
            rented++;
        } else if (vehicle.getStatus() == VehicleStatus::Maintenance) {
            maint++;
        }
    }

    total_stat_->set_text("Total: " + std::to_string(total));
    avail_stat_->set_text("Available: " + std::to_string(avail));
    rent_stat_->set_text("Rented: " + std::to_string(rented));
    maint_stat_->set_text("Maintenance: " + std::to_string(maint));
}

// async triggers

auto UI::trigger_load_fleet() -> void {
    loading_overlay_ = std::make_shared<LoadingOverlay>("Loading Fleet DB...");
    root_->add_child(loading_overlay_);
    compositor_->trigger_layout();

    worker_.enqueue([this]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::LoadFleet;
        try {
            res.fleet = this->manager_.getFleet();
            res.success = true;
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

auto UI::trigger_rent(uint32_t vehicle_id, const std::string& name, const std::string& surname,
                      const std::string& id_card) -> void {
    loading_overlay_ =
        std::make_shared<LoadingOverlay>("Renting vehicle " + std::to_string(vehicle_id) + "...");
    root_->add_child(loading_overlay_);
    compositor_->trigger_layout();

    worker_.enqueue([this, vehicle_id, name, surname, id_card]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::RentVehicle;
        res.target_id = vehicle_id;
        try {
            std::string print_warning;
            auto code =
                this->manager_.rentVehicle(vehicle_id, name, surname, id_card, &print_warning);
            if (code.has_value()) {
                res.success = true;
                res.message = code.value();
                if (!print_warning.empty()) {
                    res.message += " [Print Warning: " + print_warning + "]";
                }
            } else {
                res.success = false;
                res.message = "Vehicle is not available for rent.";
            }
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

auto UI::trigger_return(uint32_t vehicle_id, const std::string& notes) -> void {
    loading_overlay_ =
        std::make_shared<LoadingOverlay>("Returning vehicle " + std::to_string(vehicle_id) + "...");
    root_->add_child(loading_overlay_);
    compositor_->trigger_layout();

    worker_.enqueue([this, vehicle_id, notes]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::ReturnVehicle;
        res.target_id = vehicle_id;
        try {
            int cost = 0;
            std::string print_warning;
            bool success = this->manager_.returnVehicle(vehicle_id, &cost, notes, &print_warning);
            if (success) {
                res.success = true;
                std::ostringstream oss;
                oss << "Vehicle returned successfully!\nTotal Cost: " << uiFormatMoney(cost)
                    << " PLN";
                if (!print_warning.empty()) {
                    oss << "\n\nWarning: Receipt printing failed (" << print_warning << ").";
                } else {
                    oss << "\nReceipt printed.";
                }
                res.message = oss.str();
            } else {
                res.success = false;
                res.message = "Failed to return vehicle. Is it rented?";
            }
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

auto UI::trigger_return_code(const std::string& code, const std::string& notes) -> void {
    loading_overlay_ = std::make_shared<LoadingOverlay>("Returning with code " + code + "...");
    root_->add_child(loading_overlay_);
    compositor_->trigger_layout();

    worker_.enqueue([this, code, notes]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::ReturnVehicle;
        try {
            auto veh_id_opt = this->manager_.getVehicleIdByRentalCode(code);
            if (veh_id_opt.has_value()) {
                res.target_id = veh_id_opt.value();
            }
            int cost = 0;
            std::string print_warning;
            bool success = this->manager_.returnVehicle(code, &cost, notes, &print_warning);
            if (success) {
                res.success = true;
                std::ostringstream oss;
                oss << "Successfully returned rental code " << code
                    << "!\nTotal Cost: " << uiFormatMoney(cost) << " PLN";
                if (!print_warning.empty()) {
                    oss << "\n\nWarning: Receipt printing failed (" << print_warning << ").";
                } else {
                    oss << "\nReceipt printed.";
                }
                res.message = oss.str();
            } else {
                res.success = false;
                res.message = "Invalid or inactive rental code.";
            }
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

auto UI::trigger_add_vehicle(const Vehicle& vehicle) -> void {
    loading_overlay_ = std::make_shared<LoadingOverlay>("Registering vehicle...");
    root_->add_child(loading_overlay_);
    compositor_->trigger_layout();

    worker_.enqueue([this, vehicle]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::AddVehicle;
        try {
            this->manager_.addVehicle(vehicle);
            res.success = true;
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

auto UI::trigger_update_status(uint32_t vehicle_id, VehicleStatus status,
                               const std::string& notes) -> void {
    loading_overlay_ = std::make_shared<LoadingOverlay>("Updating status...");
    root_->add_child(loading_overlay_);
    compositor_->trigger_layout();

    worker_.enqueue([this, vehicle_id, status, notes]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::UpdateStatus;
        res.target_id = vehicle_id;
        try {
            bool success = this->manager_.updateVehicleStatus(vehicle_id, status, notes);
            res.success = success;
            if (!success) {
                res.message = "Vehicle ID not found.";
            }
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

auto UI::trigger_load_logs(uint32_t vehicle_id) -> void {
    worker_.enqueue([this, vehicle_id]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::LoadLogs;
        res.target_id = vehicle_id;
        try {
            // up to 25 records
            res.logs =
                this->manager_.getVehicle(vehicle_id).has_value()
                    ? std::unique_ptr<IStorage>(new FileStorage(AppPaths::dataDir().string()))
                          ->getRecordRange(vehicle_id, 0, 25)
                    : std::vector<Record>{};
            res.success = true;
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

auto UI::trigger_decommission(uint32_t vehicle_id) -> void {
    loading_overlay_ = std::make_shared<LoadingOverlay>("Decommissioning vehicle...");
    root_->add_child(loading_overlay_);
    compositor_->trigger_layout();

    worker_.enqueue([this, vehicle_id]() {
        AsyncTaskResult res;
        res.type = AsyncTaskType::DecommissionVehicle;
        res.target_id = vehicle_id;
        try {
            bool success = this->manager_.removeVehicle(vehicle_id);
            res.success = success;
            if (!success) {
                res.message = "Failed to decommission vehicle. Ensure it is not currently rented.";
            }
        } catch (const std::exception& e) {
            res.success = false;
            res.message = e.what();
        }
        this->result_queue_.push(std::move(res));
    });
}

// process results

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto UI::process_result_queue() -> void {
    auto res_opt = result_queue_.pop();
    if (!res_opt.has_value()) {
        return;
    }

    const auto& res = res_opt.value();

    if (res.type != AsyncTaskType::LoadLogs && loading_overlay_ != nullptr) {
        root_->remove_child(loading_overlay_);
        loading_overlay_ = nullptr;
    }

    switch (res.type) {
    case AsyncTaskType::LoadLogs: {
        if (selected_vehicle_id_.has_value() && selected_vehicle_id_.value() == res.target_id) {
            is_loading_logs_ = false;
            selected_vehicle_logs_ = res.logs;
            rebuild_details_panel();
            compositor_->trigger_layout();
        }
        break;
    }
    case AsyncTaskType::LoadFleet: {
        if (res.success) {
            current_fleet_ = res.fleet;
            update_stats();
            refresh_fleet_list();
        } else {
            show_alert("Database Error", "Failed to load vehicle database: " + res.message);
        }
        break;
    }
    case AsyncTaskType::RentVehicle: {
        if (res.success) {
            size_t warning_pos = res.message.find(" [Print Warning: ");
            std::string msg;
            if (warning_pos != std::string::npos) {
                std::string code = res.message.substr(0, warning_pos);
                std::string warning = res.message.substr(warning_pos + 17);
                if (!warning.empty() && warning.back() == ']') {
                    warning.pop_back();
                }
                msg = "Vehicle rented successfully!\nRental Code: " + code +
                      "\n\nWarning: Receipt printing failed (" + warning + ").";
            } else {
                msg = "Vehicle rented successfully!\nRental Code: " + res.message +
                      "\nReceipt printed.";
            }
            show_alert("Rental Complete", msg);
            if (selected_vehicle_id_.has_value() && selected_vehicle_id_.value() == res.target_id) {
                trigger_load_logs(res.target_id);
            }
        } else {
            show_alert("Rental Failed", res.message);
        }
        trigger_load_fleet();
        break;
    }
    case AsyncTaskType::ReturnVehicle: {
        if (res.success) {
            std::string msg =
                res.message.empty()
                    ? "Vehicle returned successfully.\nCost calculated & receipt printed."
                    : res.message;
            show_alert("Return Complete", msg);
            if (selected_vehicle_id_.has_value() && selected_vehicle_id_.value() == res.target_id) {
                trigger_load_logs(res.target_id);
            }
        } else {
            show_alert("Return Failed", res.message);
        }
        trigger_load_fleet();
        break;
    }
    case AsyncTaskType::AddVehicle: {
        if (res.success) {
            show_alert("Registered", "Vehicle added to fleet registry database.");
        } else {
            show_alert("Add Failed", res.message);
        }
        trigger_load_fleet();
        break;
    }
    case AsyncTaskType::UpdateStatus: {
        if (!res.success) {
            show_alert("Update Failed", res.message);
        } else {
            if (selected_vehicle_id_.has_value() && selected_vehicle_id_.value() == res.target_id) {
                trigger_load_logs(res.target_id);
            }
        }
        trigger_load_fleet();
        break;
    }
    case AsyncTaskType::DecommissionVehicle: {
        if (res.success) {
            show_alert("Decommissioned", "Vehicle has been permanently decommissioned.");
            if (selected_vehicle_id_.has_value() && selected_vehicle_id_.value() == res.target_id) {
                selected_vehicle_id_ = std::nullopt;
                selected_vehicle_logs_.clear();
                split_area_->set_split_enabled(false);
            }
        } else {
            show_alert("Decommission Failed", res.message);
        }
        trigger_load_fleet();
        break;
    }
    }
}

auto UI::show_alert(const std::string& title, const std::string& message) -> void {
    auto modal_holder = std::make_shared<std::shared_ptr<Modal>>();
    *modal_holder = std::make_shared<Modal>(title, message, [this, modal_holder](bool) {
        this->root_->remove_child(*modal_holder);
        this->compositor_->trigger_layout();
    });

    this->root_->add_child(*modal_holder);
    this->compositor_->trigger_layout();
}

auto UI::show_confirm_quit() -> void {
    if (confirm_quit_open_) {
        return;
    }
    confirm_quit_open_ = true;
    auto modal_holder = std::make_shared<std::shared_ptr<Modal>>();
    *modal_holder =
        std::make_shared<Modal>("Confirm Exit", "Are you sure you want to quit the system?",
                                [this, modal_holder](bool confirmed) {
                                    this->root_->remove_child(*modal_holder);
                                    this->confirm_quit_open_ = false;
                                    if (confirmed) {
                                        this->compositor_->quit();
                                    } else {
                                        this->compositor_->trigger_layout();
                                    }
                                });

    this->root_->add_child(*modal_holder);
    this->compositor_->trigger_layout();
}

auto UI::show_quick_return_modal() -> void {
    auto modal_holder = std::make_shared<std::shared_ptr<QuickReturnModal>>();
    *modal_holder = std::make_shared<QuickReturnModal>(
        [this, modal_holder](const std::string& code, const std::string& notes) {
            this->root_->remove_child(*modal_holder);
            this->compositor_->trigger_layout();
            if (!code.empty()) {
                this->trigger_return_code(code, notes);
            }
        });
    this->root_->add_child(*modal_holder);
    this->compositor_->trigger_layout();
}

auto UI::show_advanced_filter_modal() -> void {
    auto modal_holder = std::make_shared<std::shared_ptr<AdvancedFilterModal>>();
    *modal_holder = std::make_shared<AdvancedFilterModal>(
        advanced_filter_.value_or(AdvancedFilter{}),
        [this, modal_holder](const std::optional<AdvancedFilter>& filter_opt) {
            this->root_->remove_child(*modal_holder);
            this->compositor_->trigger_layout();
            if (filter_opt.has_value()) {
                if (filter_opt->active()) {
                    this->advanced_filter_ = filter_opt;
                    this->advanced_filter_btn_->label = "Clear filters";
                } else {
                    this->advanced_filter_ = std::nullopt;
                    this->advanced_filter_btn_->label = "Advanced filters";
                }
                this->refresh_fleet_list();
            }
        });
    this->root_->add_child(*modal_holder);
    this->compositor_->trigger_layout();
}
