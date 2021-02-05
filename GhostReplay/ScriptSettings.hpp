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
    } Main;

    struct {
        bool AutoGhost = true;
        int DeltaMillis = 0;
    } Record;

    struct {
        int VehicleAlpha = 40;
        std::string FallbackModel = "sultan";
        bool ForceFallbackModel = false;
        bool AutoLoadGhost = true;
    } Replay;

private:
    std::string mSettingsFile;
};

