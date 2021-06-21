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
        bool Debug = false;
    } Main;

    struct {
        bool AutoGhost = true;
        int DeltaMillis = 0;
    } Record;

    struct {
        // Normal
        float OffsetSeconds = 0.0f;
        int VehicleAlpha = 40;
        int ForceLights = 0;

        // Advanced
        std::string FallbackModel = "sultan";
        bool ForceFallbackModel = false;
        bool AutoLoadGhost = true;
    } Replay;

private:
    std::string mSettingsFile;
};

