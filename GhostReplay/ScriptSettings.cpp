#include "ScriptSettings.hpp"

#include "Util/Logger.hpp"

#include <simpleini/SimpleIni.h>

#define CHECK_LOG_SI_ERROR(result, operation) \
    if ((result) < 0) { \
        logger.Write(ERROR, "[Config] %s Failed to %s, SI_Error [%d]", \
        __FUNCTION__, operation, result); \
    }

CScriptSettings::CScriptSettings(std::string settingsFile)
    : mSettingsFile(std::move(settingsFile)) {
    
}

void CScriptSettings::Load() {
    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error result = ini.LoadFile(mSettingsFile.c_str());
    CHECK_LOG_SI_ERROR(result, "load");

    Main.LinesVisible = ini.GetBoolValue("Main", "LinesVisible", Main.LinesVisible);
    Main.AutoGhost = ini.GetBoolValue("Main", "AutoGhost", Main.AutoGhost);
    Main.DeltaMillis = ini.GetLongValue("Main", "DeltaMillis", Main.DeltaMillis);
}

void CScriptSettings::Save() {
    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error result = ini.LoadFile(mSettingsFile.c_str());
    CHECK_LOG_SI_ERROR(result, "load");

   ini.SetBoolValue("Main", "LinesVisible", Main.LinesVisible);
   ini.SetBoolValue("Main", "AutoGhost", Main.AutoGhost);
   ini.SetLongValue("Main", "DeltaMillis", Main.DeltaMillis);

    result = ini.SaveFile(mSettingsFile.c_str());
    CHECK_LOG_SI_ERROR(result, "save");
}
