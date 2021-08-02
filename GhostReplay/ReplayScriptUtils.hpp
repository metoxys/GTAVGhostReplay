#pragma once
#include <inc/types.h>
#include <string>

namespace Util {
    std::string StripString(std::string input);
    std::string FormatMillisTime(double totalTime);
    std::string FormatReplayName(
        double totalTime,
        const std::string& trackName,
        const std::string& vehicleName);
    std::string GetVehicleName(Hash model);
    std::string GetVehicleMake(Hash model);
    std::string GetTimestampReadable(unsigned long long unixTimestampMs);
    Vector3 GroundZ(Vector3 v, float off);
    int CreateParticleFxAtCoord(const char* assetName, const char* effectName, Vector3 coord, float r, float g, float b,
                                float a);
}
