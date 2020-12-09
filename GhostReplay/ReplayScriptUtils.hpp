#pragma once
#include <inc/types.h>
#include <string>

namespace Util {
    std::string StripString(std::string input);
    std::string FormatMillisTime(unsigned long long totalTime);
    std::string FormatReplayName(
        unsigned long long totalTime,
        const std::string& trackName,
        const std::string& vehicleName);
    std::string GetVehicleName(Hash model);
    std::string GetTimestampReadable(unsigned long long unixTimestampMs);
}
