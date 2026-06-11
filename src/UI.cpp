#include "UI.h"

#include <utility>

namespace {

auto isNavigationKey(const ncinput& input) -> bool {
	return input.id == NCKEY_TAB || input.id == NCKEY_UP || input.id == NCKEY_DOWN ||
		   input.id == NCKEY_LEFT || input.id == NCKEY_RIGHT;
}

} // namespace

auto UI::setRoot(std::shared_ptr<notui::Widget> root) -> void {
	root_ = std::move(root);
	if (root_) {
		focus_manager_.rebuild(*root_);
	}
}

auto UI::render() -> void {
	if (root_) {
		root_->render();
	}
}

auto UI::isMouseInput(const ncinput& input) -> bool {
	return input.id == NCKEY_MOTION || (input.id >= NCKEY_BUTTON1 && input.id <= NCKEY_BUTTON11);
}

auto UI::dispatchInput(const ncinput& input) -> bool {
	if (!root_) {
		return false;
	}

	if (isMouseInput(input)) {
		return root_->handle_input(input);
	}

	if (notui::Widget* focused = focus_manager_.focusedWidget(); focused != nullptr) {
		if (focused->handle_input(input)) {
			return true;
		}
	}

	if (isNavigationKey(input)) {
		return focus_manager_.handleKeyboardInput(input);
	}

	return false;
}
