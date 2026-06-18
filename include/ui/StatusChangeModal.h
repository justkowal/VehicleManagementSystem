#pragma once

#include "notui/Modal.h"
#include "notui/InputBox.h"
#include <string>
#include <functional>
#include <memory>

class StatusChangeModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> notes_in;
public:
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    StatusChangeModal(std::string title, const std::string& action_details, const std::function<void(bool, std::string)>& callback);
};
