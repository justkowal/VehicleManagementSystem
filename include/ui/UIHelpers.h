#pragma once
#include <string>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <regex>
#include <algorithm>
#include <cctype>
#include "Types.h"
#include "notui/Widget.h" 

namespace ui_helpers {

inline auto uiFormatMoney(int amount_grosz) -> std::string {
    int zlotys = amount_grosz / 100;
    int groszy = std::abs(amount_grosz % 100);
    std::ostringstream oss;
    oss << zlotys << "." << std::setw(2) << std::setfill('0') << groszy;
    return oss.str();
}

struct LogRenderInfo {
    notui::ColorRGB color;
    std::string details;
};

inline auto process_log_entry(RecordType type, const std::string& raw_details) -> LogRenderInfo {
    notui::ColorRGB color{120, 120, 120}; 
    switch (type) {
    case RecordType::Checkout:
        color = {220, 160, 20}; 
        break;
    case RecordType::Returned:
        color = {40, 180, 80}; 
        break;
    case RecordType::Maintenance:
        color = {220, 50, 50}; 
        break;
    case RecordType::Note:
        color = {80, 150, 255}; 
        break;
    }

    std::string details = raw_details;
    static const std::regex status_pattern(R"(^\[([^\]]+)\]\s*)");
    std::smatch match;
    if (std::regex_search(raw_details, match, status_pattern)) {
        std::string status_label = match[1].str();
        details = match.suffix().str();

        std::string lower_label = status_label;
        std::transform(
            lower_label.begin(), lower_label.end(), lower_label.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

        if (lower_label == "checkout" || lower_label == "rent" || lower_label == "rented") {
            color = {220, 160, 20}; 
        } else if (lower_label == "returned" || lower_label == "return" || lower_label == "retn" ||
                   lower_label == "available" || lower_label == "avail") {
            color = {40, 180, 80}; 
        } else if (lower_label == "maintenance" || lower_label == "service" ||
                   lower_label == "mntc" || lower_label == "maint") {
            color = {220, 50, 50}; 
        } else if (lower_label == "note") {
            color = {80, 150, 255}; 
        } else {
            color = {120, 120, 120}; 
        }
    }

    return {color, details};
}

} 
