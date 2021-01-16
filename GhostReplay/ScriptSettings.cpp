#include "ScriptSettings.hpp"
#include "SettingsCommon.h"

#include "Util/Logger.hpp"

#include <simpleini/SimpleIni.h>

#define CHECK_LOG_SI_ERROR(result, operation) \
    if ((result) < 0) { \
        logger.Write(ERROR, "[Config] %s Failed to %s, SI_Error [%d]", \
        __FUNCTION__, operation, result); \
    }

#define SAVE_VAL(section, key, option) \
    SetValue(ini, section, key, option)

#define LOAD_VAL(section, key, option) \
    option = GetValue(ini, section, key, option)

CScriptSettings::CScriptSettings(std::string settingsFile)
    : mSettingsFile(std::move(settingsFile)) {
    
}

void CScriptSettings::Load() {
    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error result = ini.LoadFile(mSettingsFile.c_str());
    CHECK_LOG_SI_ERROR(result, "load");

    LOAD_VAL("Main", "NotifyLaps", Main.NotifyLaps);
    LOAD_VAL("Main", "DrawStartFinish", Main.DrawStartFinish);
    LOAD_VAL("Main", "ExtensiveReplayTelemetry", Main.ExtensiveReplayTelemetry);

    LOAD_VAL("Record", "AutoGhost", Record.AutoGhost);
    LOAD_VAL("Record", "DeltaMillis", Record.DeltaMillis);

    LOAD_VAL("Replay", "VehicleAlpha", Replay.VehicleAlpha);
    LOAD_VAL("Replay", "FallbackModel", Replay.FallbackModel);
    LOAD_VAL("Replay", "ForceFallbackModel", Replay.ForceFallbackModel);
    LOAD_VAL("Replay", "AutoLoadGhost", Replay.AutoLoadGhost);
}

void CScriptSettings::Save() {
    CSimpleIniA ini;
    ini.SetUnicode();
    SI_Error result = ini.LoadFile(mSettingsFile.c_str());
    CHECK_LOG_SI_ERROR(result, "load");

    SAVE_VAL("Main", "NotifyLaps", Main.NotifyLaps);
    SAVE_VAL("Main", "DrawStartFinish", Main.DrawStartFinish);
    SAVE_VAL("Main", "ExtensiveReplayTelemetry", Main.ExtensiveReplayTelemetry);

    SAVE_VAL("Record", "AutoGhost", Record.AutoGhost);
    SAVE_VAL("Record", "DeltaMillis", Record.DeltaMillis);

    SAVE_VAL("Replay", "VehicleAlpha", Replay.VehicleAlpha);
    SAVE_VAL("Replay", "FallbackModel", Replay.FallbackModel);
    SAVE_VAL("Replay", "ForceFallbackModel", Replay.ForceFallbackModel);
    SAVE_VAL("Replay", "AutoLoadGhost", Replay.AutoLoadGhost);

    result = ini.SaveFile(mSettingsFile.c_str());
    CHECK_LOG_SI_ERROR(result, "save");
}
