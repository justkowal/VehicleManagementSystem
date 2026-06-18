#pragma once

#include "notui/VBox.h"
#include <string>

class LoadingOverlay : public notui::VBox {
public:
    explicit LoadingOverlay(std::string message);
    ~LoadingOverlay() override = default;

    LoadingOverlay(const LoadingOverlay&) = delete;
    auto operator=(const LoadingOverlay&) -> LoadingOverlay& = delete;
    LoadingOverlay(LoadingOverlay&&) = delete;
    auto operator=(LoadingOverlay&&) -> LoadingOverlay& = delete;

    auto layout(struct ncplane* parent_plane, notui::Point pos, notui::Size size) -> void override;
    auto destroy_planes() -> void override;
    auto raise_to_top() -> void override;

private:
    struct ncplane* backdrop_plane_{nullptr};
};
