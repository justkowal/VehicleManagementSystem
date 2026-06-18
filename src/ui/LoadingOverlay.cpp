#include "ui/LoadingOverlay.h"
#include "notui/Spacer.h"
#include "notui/Label.h"
#include <notcurses/notcurses.h>

using namespace notui;

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

    struct ncplane_options b_opts = {};
    b_opts.y = 0;
    b_opts.x = 0;
    b_opts.rows = static_cast<unsigned>(size.height);
    b_opts.cols = static_cast<unsigned>(size.width);
    b_opts.userptr = this;
    b_opts.name = "loading_backdrop";
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
