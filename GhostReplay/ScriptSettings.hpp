#pragma once
#include <string>

class CScriptSettings {
public:
    CScriptSettings(std::string settingsFile);

    void Load();
    void Save();

    struct {
        bool LinesVisible = true;
        bool AutoGhost = true;
        long DeltaMillis = 0;
    } Main;

private:
    std::string mSettingsFile;
};

