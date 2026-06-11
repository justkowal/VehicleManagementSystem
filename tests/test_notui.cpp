#include <catch2/catch_test_macros.hpp>
#include "notui/Widget.h"
#include "notui/Checkbox.h"
#include "notui/TextArea.h"
#include "notui/Event.h"
#include <string>
#include <any>

using namespace notui;

class MockWidget : public Widget {
public:
    void render() override {}
};


static void check_event_registration(MockWidget& widget) {
    bool event_fired = false;
    std::string received_value;

    widget.on("custom_event", [&](const Event& evt) {
        event_fired = true;
        received_value = std::any_cast<std::string>(evt.data);
    });

    widget.emit("custom_event", std::string("test_data"));

    REQUIRE(event_fired == true); // NOLINT(cppcoreguidelines-avoid-do-while)
    REQUIRE(received_value == "test_data"); // NOLINT(cppcoreguidelines-avoid-do-while)
}

static void check_multiple_listeners(MockWidget& widget) {
    int call_count = 0;

    widget.on("click", [&](const Event&) { call_count++; });
    widget.on("click", [&](const Event&) { call_count++; });

    widget.emit("click");

    REQUIRE(call_count == 2); // NOLINT(cppcoreguidelines-avoid-do-while)
}

TEST_CASE("Unified Event Dispatcher functionality", "[event_dispatcher]") {
    MockWidget widget;

    SECTION("Event listener registration and emission") {
        check_event_registration(widget);
    }

    SECTION("Multiple listeners for the same event") {
        check_multiple_listeners(widget);
    }
}

static void check_checkbox_toggle(Checkbox& checkbox) {
    bool change_fired = false;
    bool checked_state = false;

    checkbox.on("change", [&](const Event& evt) {
        change_fired = true;
        checked_state = std::any_cast<bool>(evt.data);
    });

    checkbox.set_checked(true);

    REQUIRE(checkbox.is_checked() == true); // NOLINT(cppcoreguidelines-avoid-do-while)
    REQUIRE(change_fired == true); // NOLINT(cppcoreguidelines-avoid-do-while)
    REQUIRE(checked_state == true); // NOLINT(cppcoreguidelines-avoid-do-while)

    change_fired = false;
    checkbox.set_checked(true);
    REQUIRE(change_fired == false); // NOLINT(cppcoreguidelines-avoid-do-while)
}

static void check_checkbox_space_key(Checkbox& checkbox) {
    bool change_fired = false;
    checkbox.on("change", [&](const Event& /*evt*/) { change_fired = true; });

    ncinput input = {};
    input.id = ' ';
    input.evtype = NCTYPE_PRESS;

    bool handled = checkbox.handle_input(input);

    REQUIRE(handled == true); // NOLINT(cppcoreguidelines-avoid-do-while)
    REQUIRE(checkbox.is_checked() == true); // NOLINT(cppcoreguidelines-avoid-do-while)
    REQUIRE(change_fired == true); // NOLINT(cppcoreguidelines-avoid-do-while)
}

TEST_CASE("Checkbox logic and change events", "[checkbox]") {
    Checkbox checkbox("Test Checkbox", false);

    REQUIRE(checkbox.is_checked() == false); // NOLINT(cppcoreguidelines-avoid-do-while)

    SECTION("Toggle check status programmatically") {
        check_checkbox_toggle(checkbox);
    }

    SECTION("Simulate space key toggle") {
        check_checkbox_space_key(checkbox);
    }
}

static void check_set_get_text(TextArea& textarea) {
    textarea.set_text("Hello\nWorld");
    REQUIRE(textarea.get_text() == "Hello\nWorld"); // NOLINT(cppcoreguidelines-avoid-do-while)
}

static void check_character_typing(TextArea& textarea) {
    textarea.set_text("");

    ncinput input = {};
    input.evtype = NCTYPE_PRESS;

    input.id = 'A';
    REQUIRE(textarea.handle_input(input) == true); // NOLINT(cppcoreguidelines-avoid-do-while)

    input.id = 'B';
    REQUIRE(textarea.handle_input(input) == true); // NOLINT(cppcoreguidelines-avoid-do-while)

    REQUIRE(textarea.get_text() == "AB"); // NOLINT(cppcoreguidelines-avoid-do-while)
}

static void check_backspace(TextArea& textarea) {
    textarea.set_text("C++");

    ncinput right_key = {};
    right_key.id = NCKEY_RIGHT;
    right_key.evtype = NCTYPE_PRESS;
    textarea.handle_input(right_key);
    textarea.handle_input(right_key);
    textarea.handle_input(right_key);

    ncinput backspace = {};
    backspace.id = NCKEY_BACKSPACE;
    backspace.evtype = NCTYPE_PRESS;

    REQUIRE(textarea.handle_input(backspace) == true); // NOLINT(cppcoreguidelines-avoid-do-while)
    REQUIRE(textarea.get_text() == "C+"); // NOLINT(cppcoreguidelines-avoid-do-while)
}

TEST_CASE("TextArea text buffer manipulation", "[textarea]") {
    TextArea textarea("Placeholder", Size{5, 20});

    REQUIRE(textarea.get_text() == ""); // NOLINT(cppcoreguidelines-avoid-do-while)

    SECTION("Setting and getting text") {
        check_set_get_text(textarea);
    }

    SECTION("Handling simple character typing") {
        check_character_typing(textarea);
    }

    SECTION("Handling Backspace") {
        check_backspace(textarea);
    }
}
