#pragma once
#include "VehicleMod.h"

#include <inc/types.h>
#include <string>
#include <vector>

struct SReplayNode {
    unsigned long long Timestamp; // milliseconds
    Vector3 Pos;
    Vector3 Rot; // roll/pitch/yaw, degrees
    std::vector<float> WheelRotations; // radians
    std::vector<float> SuspensionCompressions;

    float SteeringAngle; // steering input angle dependent on steering lock, radians
    float Throttle; // 0-1, percentage
    float Brake; // 0-1, percentage

    int Gear;
    float RPM; // 0-1, percentage

    bool LowBeams;
    bool HighBeams;

    float VehicleSpeed; // meters/second
    float EngineTemperature; // degrees Celsius

    bool operator<(const SReplayNode& other) const {
        return Timestamp < other.Timestamp;
    }
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

    unsigned long long Timestamp;
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
