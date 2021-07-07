#pragma once
#include <string>

class CScriptSettings {
public:
    CScriptSettings(std::string settingsFile);

    void Load();
    void Save();

    struct {
        bool NotifyLaps = true;
        bool DrawStartFinish = true;
        bool GhostBlips = true;
        bool StartStopBlips = true;
        bool ShowRecordTime = true;
        bool Debug = false;
    } Main;

    struct {
        bool AutoGhost = true;
        int DeltaMillis = 0;
        bool ReduceFileSize = true;
    } Record;

    struct {
        // Normal
        double OffsetSeconds = 0.0;
        int VehicleAlpha = 40;
        int ForceLights = 0;

        // Advanced
        double ScrubDistanceSeconds = 1.0;
        std::string FallbackModel = "sultan";
        bool ForceFallbackModel = false;
        bool AutoLoadGhost = true;
        bool ZeroVelocityOnPause = true;
    } Replay;

private:
    std::string mSettingsFile;
};

