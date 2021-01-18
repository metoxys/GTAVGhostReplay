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
    std::vector<float> SuspensionCompressions; // meters, 0 is none, positive is amount of compression

    float SteeringAngle; // steering input angle dependent on steering lock, radians
    float Throttle; // 0-1, percentage
    float Brake; // 0-1, percentage

    int Gear;
    float RPM; // 0-1, percentage

    bool LowBeams;
    bool HighBeams;

    float VehicleSpeed; // meters/second
    float EngineTemperature; // degrees Celsius
    float Turbo; // -1-1, percentage, -0.9 is idle while stationary, -1.0 is idle while moving
    float SteeringInputAngle; // -1-1, percentage, positive is to the left
    // float VisualHeight; // suspension upgrade impact, meters, positive is lowered car
    std::vector<float> WheelSpeeds; // meters/second
    std::vector<float> WheelPowers; // g
    std::vector<float> WheelBrakes; // g
    std::vector<float> WheelDownforces; // g
    std::vector<float> WheelTractions; // g
    std::vector<float> WheelRotationSpeeds; // radians/second
    std::vector<float> WheelSteeringAngles; // radians, positive is to the left
    Vector3 VehicleSpeedVector; // left/forwards/upwards, meters/second
    Vector3 VehicleVelocity; // meters/second
    float Clutch; // 0-1, percentage
    float GForceLat; // g, positive is to the right (like turning left)
    float GForceLon; // g, positive is backwards (accelerating)
    float GForceVert; // g, positive is upwards (like driving into a hill and having the ground push you away), negative is downwards (like driving over a crest)
    std::vector<bool> WheelsOnGround; // ?

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
