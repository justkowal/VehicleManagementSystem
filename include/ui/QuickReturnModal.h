#pragma once

#include "notui/Modal.h"
#include "notui/InputBox.h"
#include <string>
#include <functional>
#include <memory>

class QuickReturnModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> code_in;
    std::shared_ptr<notui::InputBox<std::string>> notes_in;
public:
    explicit QuickReturnModal(const std::function<void(std::string, std::string)>& callback);
};
