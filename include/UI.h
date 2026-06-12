#pragma once

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <optional>
#include "FleetMgmt.h"
#include "notui/Widget.h"
#include "notui/Compositor.h"
#include "notui/VBox.h"
#include "notui/HBox.h"
#include "notui/SplitBox.h"
#include "notui/Spacer.h"
#include "notui/Label.h"
#include "notui/Button.h"
#include "notui/ScrollArea.h"
#include "notui/Dropdown.h"
#include "notui/InputBox.h"
#include "notui/Modal.h"

enum class AsyncTaskType : uint8_t {
    LoadFleet,
    RentVehicle,
    ReturnVehicle,
    AddVehicle,
    UpdateStatus,
    LoadLogs,
    DecommissionVehicle
};

struct AsyncTaskResult {
    AsyncTaskType type{AsyncTaskType::LoadFleet};
    bool success{false};
    std::string message;
    uint32_t target_id{0};
    std::vector<Vehicle> fleet;
    std::vector<Record> logs;
};

class TaskResultQueue {
public:
    auto push(AsyncTaskResult res) -> void {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(res));
    }

    [[nodiscard]] auto pop() -> std::optional<AsyncTaskResult> {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        AsyncTaskResult res = std::move(queue_.front());
        queue_.pop();
        return res;
    }

private:
    std::queue<AsyncTaskResult> queue_;
    std::mutex mutex_;
};

class BackgroundWorker {
public:
    BackgroundWorker() {
        worker_thread_ = std::thread(&BackgroundWorker::run, this);
    }

    ~BackgroundWorker() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    BackgroundWorker(const BackgroundWorker&) = delete;
    auto operator=(const BackgroundWorker&) -> BackgroundWorker& = delete;
    BackgroundWorker(BackgroundWorker&&) = delete;
    auto operator=(BackgroundWorker&&) -> BackgroundWorker& = delete;

    auto enqueue(std::function<void()> task) -> void {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

private:
    std::thread worker_thread_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_{false};

    auto run() -> void {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
                if (stop_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }
};

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

class RegisterVehicleModal : public notui::Modal {
public:
    RegisterVehicleModal(const std::function<void(std::optional<Vehicle>)>& callback);
};

class QuickReturnModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> code_in;
    std::shared_ptr<notui::InputBox<std::string>> notes_in;
public:
    QuickReturnModal(const std::function<void(std::string, std::string)>& callback);
};

class RentVehicleModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> name_in;
    std::shared_ptr<notui::InputBox<std::string>> surname_in;
    std::shared_ptr<notui::InputBox<std::string>> id_card_in;
public:
    RentVehicleModal(const Vehicle& vehicle, const std::function<void(bool, std::string, std::string, std::string)>& callback);
};

class StatusChangeModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> notes_in;
public:
    StatusChangeModal(std::string title, const std::string& action_details, const std::function<void(bool, std::string)>& callback);
};

class AdvancedFilterModal : public notui::Modal {
private:
    std::shared_ptr<notui::InputBox<std::string>> id_min;
    std::shared_ptr<notui::InputBox<std::string>> id_max;
    std::shared_ptr<notui::InputBox<std::string>> id_eq;

    std::shared_ptr<notui::InputBox<std::string>> brand_eq;

    std::shared_ptr<notui::InputBox<std::string>> model_eq;

    std::shared_ptr<notui::InputBox<std::string>> type_eq;

    std::shared_ptr<notui::InputBox<std::string>> status_eq;

    std::shared_ptr<notui::InputBox<std::string>> price_min;
    std::shared_ptr<notui::InputBox<std::string>> price_max;
    std::shared_ptr<notui::InputBox<std::string>> price_eq;

    std::shared_ptr<notui::InputBox<std::string>> seats_min;
    std::shared_ptr<notui::InputBox<std::string>> seats_max;
    std::shared_ptr<notui::InputBox<std::string>> seats_eq;

    std::shared_ptr<notui::InputBox<std::string>> payload_min;
    std::shared_ptr<notui::InputBox<std::string>> payload_max;
    std::shared_ptr<notui::InputBox<std::string>> payload_eq;

    std::shared_ptr<notui::InputBox<std::string>> rental_code_eq;

public:
    AdvancedFilterModal(const AdvancedFilter& current_filter, const std::function<void(std::optional<AdvancedFilter>)>& callback);
};

class VehicleCard : public notui::VBox {
public:
    uint32_t vehicle_id;
    explicit VehicleCard(uint32_t veh_id) : vehicle_id(veh_id) {
        focusable = true;
    }
    ~VehicleCard() override = default;

    VehicleCard(const VehicleCard&) = delete;
    auto operator=(const VehicleCard&) -> VehicleCard& = delete;
    VehicleCard(VehicleCard&&) = delete;
    auto operator=(VehicleCard&&) -> VehicleCard& = delete;
};

class UI {
public:
    explicit UI(FleetManager& manager);
    ~UI() = default;

    UI(const UI&) = delete;
    auto operator=(const UI&) -> UI& = delete;
    UI(UI&&) = delete;
    auto operator=(UI&&) -> UI& = delete;

    auto run() -> void;

private:
    FleetManager& manager_;
    std::shared_ptr<notui::VBox> root_;
    std::shared_ptr<notui::Compositor> compositor_;

    BackgroundWorker worker_;
    TaskResultQueue result_queue_;

    // layout panels
    std::shared_ptr<notui::ScrollArea> left_list_panel_;
    std::shared_ptr<notui::VBox> right_detail_panel_;
    std::shared_ptr<notui::SplitBox> split_area_;

    // top bar inputs
    std::shared_ptr<notui::InputBox<std::string>> search_input_;
    std::shared_ptr<notui::Dropdown> type_filter_;
    std::shared_ptr<notui::Dropdown> status_filter_;
    std::shared_ptr<notui::Dropdown> secondary_info_filter_;
    std::shared_ptr<notui::Button> advanced_filter_btn_;

    // sidebar/details controls
    std::shared_ptr<LoadingOverlay> loading_overlay_;
    
    // stats labels
    std::shared_ptr<notui::Label> total_stat_;
    std::shared_ptr<notui::Label> avail_stat_;
    std::shared_ptr<notui::Label> rent_stat_;
    std::shared_ptr<notui::Label> maint_stat_;

    // ui state
    std::vector<Vehicle> current_fleet_;
    std::optional<uint32_t> selected_vehicle_id_;
    std::vector<Record> selected_vehicle_logs_;
    bool is_loading_logs_{false};
    bool confirm_quit_open_{false};
    std::optional<AdvancedFilter> advanced_filter_;

    auto build_ui() -> void;
    auto refresh_fleet_list() -> void;
    auto rebuild_details_panel() -> void;
    auto select_vehicle(uint32_t vehicle_id) -> void;
    auto update_stats() -> void;
    [[nodiscard]] auto handle_global_shortcuts(const ncinput& nc_input) -> bool;
    
    // async triggers
    auto trigger_load_fleet() -> void;
    auto trigger_rent(uint32_t vehicle_id, const std::string& name, const std::string& surname, const std::string& id_card) -> void;
    auto trigger_return(uint32_t vehicle_id, const std::string& notes) -> void;
    auto trigger_return_code(const std::string& code, const std::string& notes) -> void;
    auto trigger_add_vehicle(const Vehicle& vehicle) -> void;
    auto trigger_update_status(uint32_t vehicle_id, VehicleStatus status, const std::string& notes) -> void;
    auto trigger_load_logs(uint32_t vehicle_id) -> void;
    auto trigger_decommission(uint32_t vehicle_id) -> void;

    // async result handlers
    auto process_result_queue() -> void;
    auto show_alert(const std::string& title, const std::string& message) -> void;
    auto show_confirm_quit() -> void;
    auto show_quick_return_modal() -> void;
    auto show_advanced_filter_modal() -> void;
};
