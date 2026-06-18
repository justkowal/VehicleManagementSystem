#include "ui/LogsModal.h"
#include "ui/UIHelpers.h"
#include "notui/HBox.h"
#include "notui/Spacer.h"
#include <notcurses/notcurses.h>
#include <ctime>
#include <array>

using namespace notui;

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

            auto log_info = ui_helpers::process_log_entry(rec.type, rec.details);

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

    struct ncplane_options b_opts = {};
    b_opts.y = 0;
    b_opts.x = 0;
    b_opts.rows = static_cast<unsigned>(size.height);
    b_opts.cols = static_cast<unsigned>(size.width);
    b_opts.userptr = this;
    b_opts.name = "logs_backdrop";
    b_opts.resizecb = nullptr;
    b_opts.flags = 0;
    b_opts.margin_b = 0;
    b_opts.margin_r = 0;

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
