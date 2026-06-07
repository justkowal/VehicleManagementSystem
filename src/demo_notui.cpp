#include <notui/VBox.h>
#include <notui/HBox.h>
#include <notui/Button.h>
#include <notui/InputBox.h>
#include <notui/Frame.h>
#include <notui/Label.h>
#include <notui/Spacer.h>
#include <notui/SplitContainer.h>
#include <notui/FocusManager.h>

#include <notcurses/notcurses.h>
#include <memory>
#include <string>
#include <vector>

using namespace notui;

auto main() -> int {
    notcurses_options opts = {};
    opts.flags = NCOPTION_SUPPRESS_BANNERS | NCOPTION_INHIBIT_SETLOCALE; 
    struct notcurses* ncurses_instance = notcurses_core_init(&opts, nullptr);
    
    if (ncurses_instance == nullptr) {
        return EXIT_FAILURE;
    }

    // Enable all mouse events
    notcurses_mice_enable(ncurses_instance, NCMICE_ALL_EVENTS);

    struct ncplane* stdn = notcurses_stdplane(ncurses_instance);
    unsigned dim_y = 0;
    unsigned dim_x = 0;
    ncplane_dim_yx(stdn, &dim_y, &dim_x);

    FocusManager focus_manager;
    bool is_running = true;

    auto root = std::make_unique<VBox>(stdn, 0, 0, static_cast<int>(dim_y), static_cast<int>(dim_x));
    root->setFocusManager(&focus_manager);

    auto header_frame = std::make_unique<Frame>(root->getPlane(), 0, 0, 3, static_cast<int>(dim_x), " Fleet Management ");
    header_frame->setHeightPolicy(SizeMode::Fixed, 3);
    header_frame->setChild(std::make_unique<Label>(header_frame->getPlane(), " Dashboard ", TextAlignment::Center));
    root->addChild(std::move(header_frame));

    auto split = std::make_unique<SplitContainer>(root->getPlane(), SplitDirection::Horizontal);
    split->setSplitRatio(0.20F); 

    auto sidebar = std::make_unique<VBox>(split->getPlane(), 0, 0, 10, 10);
    sidebar->addChild(std::make_unique<Button>(sidebar->getPlane(), "Dashboard", [](){}));
    sidebar->addChild(std::make_unique<Button>(sidebar->getPlane(), "Active Fleet", [](){}));
    sidebar->addChild(std::make_unique<Spacer>(sidebar->getPlane())); 
    
    auto exit_btn = std::make_unique<Button>(sidebar->getPlane(), "Exit (ESC)", [&](){ is_running = false; });
    sidebar->addChild(std::move(exit_btn));

    auto content_area = std::make_unique<VBox>(split->getPlane(), 0, 0, 10, 10);
    auto grid_frame = std::make_unique<Frame>(content_area->getPlane(), 0, 0, 10, 10, " Fleet Assets ");
    auto grid_vbox = std::make_unique<VBox>(grid_frame->getPlane(), 0, 0, 10, 10);
    
    std::vector<std::string> vehicles = {"Newag Griffin", "Solaris Urbino", "Pesa Dart"};
    for (const auto& vehicle_name : vehicles) {
        auto row = std::make_unique<Button>(grid_vbox->getPlane(), vehicle_name, [](){});
        row->setHeightPolicy(SizeMode::Fixed, 1);
        grid_vbox->addChild(std::move(row));
    }
    grid_frame->setChild(std::move(grid_vbox));
    content_area->addChild(std::move(grid_frame));

    split->setFirst(std::move(sidebar));
    split->setSecond(std::move(content_area));
    root->addChild(std::move(split));

    root->resizeAndMove(0, 0, static_cast<int>(dim_y), static_cast<int>(dim_x));
    focus_manager.rebuild(*root);

    ncinput input_event;
    while (is_running) {
        root->render();
        notcurses_render(ncurses_instance);

        uint32_t key = notcurses_get_blocking(ncurses_instance, &input_event);
        
        if (input_event.evtype == NCTYPE_PRESS || input_event.evtype == NCTYPE_RELEASE) {
            // Adjust coordinates: translate absolute mouse screen position to plane-relative coordinates
            int plane_y, plane_x;
            ncplane_yx(root->getPlane(), &plane_y, &plane_x);
            
            ncinput adjusted_event = input_event;
            adjusted_event.y -= plane_y;
            adjusted_event.x -= plane_x;
            
            root->handleInput(adjusted_event);
        }

        if (key == NCKEY_ESC) {
            is_running = false;
        } else if (key == NCKEY_RESIZE) {
            ncplane_dim_yx(stdn, &dim_y, &dim_x);
            root->resizeAndMove(0, 0, static_cast<int>(dim_y), static_cast<int>(dim_x));
            focus_manager.rebuild(*root);
            continue;
        } else {
            focus_manager.handleKeyboardInput(input_event);
        }
    }

    root.reset();
    notcurses_stop(ncurses_instance);
    return EXIT_SUCCESS;
}