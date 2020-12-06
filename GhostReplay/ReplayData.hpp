#pragma once
#include <inc/types.h>
#include <string>
#include <vector>
#include <map>

#include "VehicleMod.h"

struct SReplayNode {
    unsigned long long Timestamp;
    Vector3 Pos;
    Vector3 Rot;
    float SteeringAngle;
    float Throttle;
    float Brake;

    float RPM;

    bool LowBeams;
    bool HighBeams;
};

class CReplayData {
public:
    static CReplayData Read(const std::string& replayFile);

    CReplayData() = default;
    void Write();

    std::string Name;
    std::string Track;

    Hash VehicleModel;
    Hash DriverModel;

    // TODO: Car tuning stuff
    // TODO: Maybe selectable

    // Nodes.front() shall be used to figure out which CReplayDatas apply to a given starting point.
    // The menu shall be used to select the one that applies, so multiple CReplayData recordings can be
    // chosen from.
    std::vector<SReplayNode> Nodes;
    VehicleModData VehicleMods;
};
