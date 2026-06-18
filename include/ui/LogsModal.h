#pragma once

#include "Types.h"
#include "notui/VBox.h"
#include "notui/Label.h"
#include "notui/ScrollArea.h"
#include "notui/Button.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

class LogsModal : public notui::VBox {
public:
    LogsModal(std::string title, const std::vector<Record>& records, std::function<void()> on_close);
    ~LogsModal() override = default;

    LogsModal(const LogsModal&) = delete;
    auto operator=(const LogsModal&) -> LogsModal& = delete;
    LogsModal(LogsModal&&) = delete;
    auto operator=(LogsModal&&) -> LogsModal& = delete;

    auto layout(struct ncplane* parent_plane, notui::Point pos, notui::Size size) -> void override;
    auto destroy_planes() -> void override;
    auto raise_to_top() -> void override;

private:
    std::shared_ptr<notui::Label> title_label_;
    std::shared_ptr<notui::ScrollArea> scroll_area_;
    std::shared_ptr<notui::Button> close_btn_;
    struct ncplane* backdrop_plane_{nullptr};
    std::function<void()> on_close_;
};
