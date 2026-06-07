#pragma once

#include <memory>

#include <notcurses/notcurses.h>

#include "notui/FocusManager.h"
#include "notui/Widget.h"

class UI {
public:
	auto setRoot(std::unique_ptr<notui::Widget> root) -> void;
	auto render() -> void;
	auto dispatchInput(const ncinput& input) -> bool;

	[[nodiscard]] auto focusManager() -> notui::FocusManager& { return focus_manager_; }
	[[nodiscard]] auto root() const -> notui::Widget* { return root_.get(); }

private:
	std::unique_ptr<notui::Widget> root_;
	notui::FocusManager focus_manager_;

	[[nodiscard]] static auto isMouseInput(const ncinput& input) -> bool;
};
