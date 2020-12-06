#include "ReplayScriptUtils.hpp"
#include <inc/natives.h>
#include <fmt/format.h>

namespace {
    const std::string illegalChars = "\\/:?\"<>|";
}

std::string Util::StripString(std::string input) {
    auto it = input.begin();
    for (it = input.begin(); it < input.end(); ++it) {
        bool found = illegalChars.find(*it) != std::string::npos;
        if (found) {
            *it = '.';
        }
    }
    return input;
}

std::string Util::FormatMillisTime(unsigned long long totalTime) {
    unsigned long long milliseconds = totalTime % 1000;
    unsigned long long seconds = (totalTime / 1000) % 60;
    unsigned long long minutes = (totalTime / 1000) / 60;
    return fmt::format("{}:{:02d}.{:03d}", minutes, seconds, milliseconds);
}

std::string Util::FormatReplayName(
    unsigned long long totalTime,
    const std::string& trackName,
    const std::string& vehicleName) {
    std::string timeStr = Util::FormatMillisTime(totalTime);

    return fmt::format("{} - {} - {}", trackName, vehicleName, timeStr);
}

std::string Util::GetVehicleName(Hash model) {
    const char* name = VEHICLE::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL(model);
    std::string displayName = HUD::_GET_LABEL_TEXT(name);
    if (displayName == "NULL") {
        displayName = name;
    }
    return displayName;
}
