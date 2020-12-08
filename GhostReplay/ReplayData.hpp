#pragma once
#include <mutex>

#include "VehicleMod.h"

#include <inc/types.h>
#include <string>
#include <vector>

struct SReplayNode {
    unsigned long long Timestamp;
    Vector3 Pos;
    Vector3 Rot;
    std::vector<float> WheelRotations;

    float SteeringAngle;
    float Throttle;
    float Brake;

    int Gear;
    float RPM;

    bool LowBeams;
    bool HighBeams;
};

class CReplayData {
public:
    static CReplayData Read(const std::string& replayFile);
    static void WriteAsync(const CReplayData& replayData);

    CReplayData(std::string fileName);
    void Write();
    std::string FileName() const { return mFileName; }
    void Delete() const;

    bool MarkedForDeletion;

    std::string Name;
    std::string Track;

    Hash VehicleModel;
    VehicleModData VehicleMods;

    // Nodes.front() shall be used to figure out which CReplayDatas apply to a given starting point.
    // The menu shall be used to select the one that applies, so multiple CReplayData recordings can be
    // chosen from.
    std::vector<SReplayNode> Nodes;
private:
    std::string mFileName;
};
