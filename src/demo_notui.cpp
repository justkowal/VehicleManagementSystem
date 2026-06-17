#include <notcurses/notcurses.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <algorithm>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <numbers>

#include "notui/Style.h"
#include "notui/Container.h"
#include "notui/VBox.h"
#include "notui/HBox.h"
#include "notui/Spacer.h"
#include "notui/Label.h"
#include "notui/Button.h"
#include "notui/InputBox.h"
#include "notui/ScrollArea.h"
#include "notui/Compositor.h"
#include "notui/Dropdown.h"
#include "notui/Modal.h"
#include "notui/Checkbox.h"
#include "notui/TextArea.h"
#include "notui/Canvas.h"

using namespace notui;

struct RadarBlip { double x, y; };

// bundles radar pixel state
struct RadarPixel {
    double delta_x   = 0.0;
    double delta_y   = 0.0;
    double dist      = 0.0;
    uint8_t& grn; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    uint8_t& blu; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

struct RadarFrame {
    double theta  = 0.0;
    double PI_VAL = std::numbers::pi;
    double center_x = 0.0;
    double center_y = 0.0;
};

static void apply_radar_disc(RadarPixel& rad_pixel, const RadarFrame& radar_frame) {
    if (std::abs(rad_pixel.dist - 9.0) < 0.4 ||
        std::abs(rad_pixel.dist - 18.0) < 0.4 ||
        std::abs(rad_pixel.dist - 27.0) < 0.4) {
        rad_pixel.grn += 15;
        rad_pixel.blu += 25;
    }
    if (std::abs(rad_pixel.delta_x) < 0.45 || std::abs(rad_pixel.delta_y) < 0.45) {
        rad_pixel.grn += 10;
        rad_pixel.blu += 20;
    }
    double pixel_angle = std::atan2(rad_pixel.delta_y, rad_pixel.delta_x);
    double diff = radar_frame.theta - pixel_angle;
    if (diff < 0) { 
        diff += 2.0 * radar_frame.PI_VAL; 
    }
    if (diff < radar_frame.PI_VAL / 2.0) {
        double intensity = 1.0 - (diff / (radar_frame.PI_VAL / 2.0));
        rad_pixel.grn += static_cast<uint8_t>(40 * intensity);
        rad_pixel.blu += static_cast<uint8_t>(65 * intensity);
    }
}

static void apply_blip_glow(RadarPixel& rad_pixel, const RadarBlip& blip,
                             const RadarFrame& radar_frame) {
    double blip_dx = rad_pixel.delta_x - (blip.x - radar_frame.center_x);
    double blip_dy = rad_pixel.delta_y - (blip.y - radar_frame.center_y);
    if (std::sqrt(blip_dx * blip_dx + blip_dy * blip_dy) >= 1.4) { 
        return; 
    }

    double blip_angle = std::atan2(blip.y - radar_frame.center_y, blip.x - radar_frame.center_x);
    double b_diff = radar_frame.theta - blip_angle;
    if (b_diff < 0) { 
        b_diff += 2.0 * radar_frame.PI_VAL; 
    }

    if (b_diff < radar_frame.PI_VAL) {
        double glow = 1.0 - (b_diff / radar_frame.PI_VAL);
        rad_pixel.grn += static_cast<uint8_t>(160 * glow);
        rad_pixel.blu += static_cast<uint8_t>(200 * glow);
    } else {
        rad_pixel.grn += 20;
        rad_pixel.blu += 30;
    }
}

class DemoCanvas : public Canvas {
private:
    std::vector<uint32_t> buffer;
    int frame = 0;
    bool enabled = true;

public:
    void set_enabled(bool value) { enabled = value; }
    [[nodiscard]] auto get_enabled() const -> bool { return enabled; }

    explicit DemoCanvas(Size fixed_size) : Canvas("", fixed_size) {
        style.bg({20, 20, 25}).frame(true, true, " Live Framebuffer ").pad({0, 1, 0, 1});
    }

    void render() override {
        if (!enabled) {
            set_framebuffer(nullptr, 0, 0);
            Canvas::render();
            return;
        }

        const int p_width  = 120;
        const int p_height = 60;
        const size_t buf_size = static_cast<size_t>(p_width) * static_cast<size_t>(p_height);
        if (buffer.size() != buf_size) { 
            buffer.resize(buf_size); 
        }

        const RadarFrame radar_frame = {
            .theta   = std::fmod(frame * 0.02, 6.283185307179586),
            .PI_VAL  = std::numbers::pi,
            .center_x = p_width  / 2.0,
            .center_y = p_height / 2.0
        };

        const std::vector<RadarBlip> blips = {
            {radar_frame.center_x - 20, radar_frame.center_y - 8},
            {radar_frame.center_x + 24, radar_frame.center_y + 10},
            {radar_frame.center_x + 6,  radar_frame.center_y - 16},
            {radar_frame.center_x - 28, radar_frame.center_y + 12}
        };

        for (int row = 0; row < p_height; ++row) {
            for (int col = 0; col < p_width; ++col) {
                uint8_t red = 10;
                uint8_t grn = 14;
                uint8_t blu = 22;

                RadarPixel rad_pixel = {
                    .delta_x = col - radar_frame.center_x,
                    .delta_y = row - radar_frame.center_y,
                    .dist    = 0.0,
                    .grn     = grn,
                    .blu     = blu
                };
                rad_pixel.dist = std::sqrt(rad_pixel.delta_x * rad_pixel.delta_x + rad_pixel.delta_y * rad_pixel.delta_y);

                if (rad_pixel.dist < 28.0) { 
                    apply_radar_disc(rad_pixel, radar_frame); 
                }
                for (const auto& blip : blips) { 
                    apply_blip_glow(rad_pixel, blip, radar_frame); 
                }

                const uint8_t alpha = 255;
                buffer[static_cast<size_t>(row) * static_cast<size_t>(p_width) + static_cast<size_t>(col)] =
                    (static_cast<uint32_t>(alpha) << 24) |
                    (static_cast<uint32_t>(blu)   << 16) |
                    (static_cast<uint32_t>(grn)   <<  8) |
                    red;
            }
        }
        frame++;
        set_framebuffer(buffer.data(), p_height, p_width);
        Canvas::render();
    }
};


auto main() -> int {
    auto root = std::make_shared<VBox>();
    root->style.bg({15, 15, 20}).pad({1, 1, 1, 1});

    auto header = std::make_shared<Label>("Vehicle Management System v1.1", Size{3, 0}, true);
    header->style.bg({40, 80, 200}).fg({255, 255, 255}).frame(true, true).attr(NCSTYLE_BOLD);
    header->flex = 0;
    root->add_child(header);

    auto sp1 = std::make_shared<Spacer>();
    sp1->fixed_height = 1; 
    sp1->flex = 0;
    root->add_child(sp1);

    auto main_area = std::make_shared<HBox>();
    main_area->flex = 1;

    auto sidebar = std::make_shared<VBox>();
    sidebar->fixed_width = 30;
    sidebar->style.bg({30, 30, 35}).frame(true, true, " Sidebar ").pad({1, 2, 1, 2});
    
    auto info_label = std::make_shared<Label>("Vehicle DB Controls:\nSpecify quantity to generate cards.\nUse Tab / Arrows to navigate focus.", Size{3, 26});
    info_label->style.fg({150, 150, 150});
    sidebar->add_child(info_label);

    auto sp_mid1 = std::make_shared<Spacer>(); 
    sp_mid1->flex = 0; 
    sp_mid1->fixed_height = 1;
    sidebar->add_child(sp_mid1);

    auto type_filter = std::make_shared<Dropdown>(std::vector<std::string>{"All Categories", "Sedan Cars", "Heavy Trucks", "Electric Bikes"});
    sidebar->add_child(type_filter);
    
    auto sp_mid2 = std::make_shared<Spacer>(); 
    sp_mid2->flex = 0; 
    sp_mid2->fixed_height = 1;
    sidebar->add_child(sp_mid2);

    sidebar->add_child(std::make_shared<Label>("Generate Records", Size{1, 20}));
    
    auto count_input = std::make_shared<InputBox<int>>("Qty...", 20, [](const std::string& str) {
        if(str.empty()) { 
            return true; 
        }
        return std::all_of(str.begin(), str.end(), ::isdigit);
    });
    sidebar->add_child(count_input);

    auto sp_mid3 = std::make_shared<Spacer>(); 
    sp_mid3->flex = 0; 
    sp_mid3->fixed_height = 1;
    sidebar->add_child(sp_mid3);

    auto holo_check = std::make_shared<Checkbox>("Blueprint View", true);
    sidebar->add_child(holo_check);

    auto sp_mid4 = std::make_shared<Spacer>(); 
    sp_mid4->flex = 0; 
    sp_mid4->fixed_height = 1;
    sidebar->add_child(sp_mid4);

    sidebar->add_child(std::make_shared<Label>("Operator Notes:", Size{1, 20}));
    auto notes_area = std::make_shared<TextArea>("Type notes here...", Size{4, 22});
    notes_area->style.frame(true, true);
    sidebar->add_child(notes_area);

    auto sidebar_bottom_spacer = std::make_shared<Spacer>();
    sidebar_bottom_spacer->flex = 1;
    sidebar->add_child(sidebar_bottom_spacer);

    auto right_panel = std::make_shared<VBox>();
    right_panel->flex = 1;

    auto canvas = std::make_shared<DemoCanvas>(Size{12, 0});
    right_panel->add_child(canvas);

    auto right_sp = std::make_shared<Spacer>();
    right_sp->fixed_height = 1; 
    right_sp->flex = 0;
    right_panel->add_child(right_sp);

    auto dashboard_scroll = std::make_shared<ScrollArea>();
    dashboard_scroll->style.bg({25, 25, 30}).frame(true, true, " Interactive Database ").pad({1, 2, 1, 2});
    dashboard_scroll->flex = 1;
    right_panel->add_child(dashboard_scroll);

    Compositor* app_ptr = nullptr;
    
    auto generate_btn = std::make_shared<Button>("Generate", [&]() {
        int qty = count_input->get_value();
        if (qty <= 0) { 
            qty = 5; 
        }

        for (int iter = 0; iter < qty; ++iter) {
            auto card = std::make_shared<VBox>();
            card->fixed_height = 4;
            card->style.bg({35, 35, 45}).pad({0, 1, 0, 1}).frame(true, true);
            
            auto title = std::make_shared<Label>("Record #" + std::to_string(rand() % 9000 + 1000), Size{1, 15});
            card->add_child(title);
            
            auto row = std::make_shared<HBox>();
            row->fixed_height = 1;
            row->flex = 0;
            row->main_axis_alignment = MainAxisAlignment::SpaceBetween;
            
            row->add_child(std::make_shared<Label>("Status:", Size{1, 8}));
            
            auto status_input = std::make_shared<InputBox<std::string>>("Pending...", 12);
            row->add_child(status_input);
            
            auto update_btn = std::make_shared<Button>("Save", [](){});
            update_btn->style.bg({40, 120, 60}).fg({255, 255, 255});
            row->add_child(update_btn);
            
            auto delete_btn = std::make_shared<Button>("Delete", [dashboard_scroll, card, &app_ptr]() {
                dashboard_scroll->remove_child(card);
                if (app_ptr != nullptr) { 
                    app_ptr->trigger_layout(); 
                }
            });
            delete_btn->style.bg({140, 50, 50}).fg({255, 255, 255});
            row->add_child(delete_btn);
            
            card->add_child(row);
            dashboard_scroll->add_child(card);
        }
        if (app_ptr != nullptr) { 
            app_ptr->trigger_layout(); 
        }
    });
    generate_btn->style.bg({40, 120, 60}).fg({255, 255, 255});
    sidebar->add_child(generate_btn);

    auto sp3 = std::make_shared<Spacer>(); 
    sp3->flex = 0; 
    sp3->fixed_height = 1;
    sidebar->add_child(sp3);

    auto quit_btn = std::make_shared<Button>("Exit App", nullptr);
    quit_btn->style.bg({140, 50, 50}).fg({255, 255, 255}); 
    sidebar->add_child(quit_btn);

    main_area->add_child(sidebar);
    main_area->add_child(right_panel);
    root->add_child(main_area);

    Compositor app(root);
    app_ptr = &app;

    holo_check->on("change", [canvas, &app](const Event& evt) {
        bool show_holo = std::any_cast<bool>(evt.data);
        canvas->set_enabled(show_holo);
        app.trigger_layout();
    });
    
    std::shared_ptr<Modal> exit_modal = nullptr;
    quit_btn->on_click = [&]() {
        if (exit_modal != nullptr) { 
            return; 
        }
        exit_modal = std::make_shared<Modal>("Confirm Exit", "Are you sure you want to exit the system?", [&](bool confirmed) {
            if (confirmed) {
                app.quit();
            } else {
                root->remove_child(exit_modal);
                exit_modal = nullptr;
                app.trigger_layout();
            }
        });
        root->add_child(exit_modal);
        app.trigger_layout();
    };

    app.run();

    return 0;
}