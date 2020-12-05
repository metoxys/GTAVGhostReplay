#include "Script.hpp"

#include "ReplayScript.hpp"
#include "ScriptMenu.hpp"
#include "Constants.hpp"
#include "Compatibility.h"

#include "Util/Logger.hpp"
#include "Util/Paths.hpp"
#include "Util/String.hpp"

#include <inc/natives.h>
#include <inc/main.h>
#include <fmt/format.h>
#include <memory>
#include <filesystem>

using namespace GhostReplay;
namespace fs = std::filesystem;

namespace {
    std::shared_ptr<CScriptSettings> settings;
    std::shared_ptr<CReplayScript> scriptInst;

    std::vector<CReplayData> replays;
}

void GhostReplay::ScriptMain() {
    const std::string settingsGeneralPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\settings_general.ini";
    const std::string settingsMenuPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\settings_menu.ini";

    settings = std::make_shared<CScriptSettings>(settingsGeneralPath);
    settings->Load();
    logger.Write(INFO, "Settings loaded");

    GhostReplay::LoadReplays();

    scriptInst = std::make_shared<CReplayScript>(*settings, replays);

    VehicleExtensions::Init();
    Compatibility::Setup();

    CScriptMenu menu(settingsMenuPath, 
        []() {
            // OnInit
            settings->Load();
            GhostReplay::LoadReplays();
        },
        []() {
            // OnExit
            settings->Save();
        },
        BuildMenu()
    );

    while(true) {
        scriptInst->Tick();
        menu.Tick(*scriptInst);
        WAIT(0);
    }
}

CScriptSettings& GhostReplay::GetSettings() {
    return *settings;
}

CReplayScript* GhostReplay::GetScript() {
    return scriptInst.get();
}


const std::vector<CReplayData>& GhostReplay::GetConfigs() {
    return replays;
}

uint32_t GhostReplay::LoadReplays() {
    const std::string configsPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\Configs";

    logger.Write(DEBUG, "Clearing and reloading configs");

    replays.clear();

    if (!(fs::exists(fs::path(configsPath)) && fs::is_directory(fs::path(configsPath)))) {
        logger.Write(ERROR, "Directory [%s] not found!", configsPath.c_str());
        return 0;
    }

    for (const auto& file : fs::directory_iterator(configsPath)) {
        if (Util::to_lower(fs::path(file).extension().string()) != ".json") {
            logger.Write(DEBUG, "Skipping [%s] - not .json", file.path().c_str());
            continue;
        }

        CReplayData config = CReplayData::Read(fs::path(file).string());
        if (!config.Nodes.empty())
            replays.push_back(config);
        else
            logger.Write(WARN, "Skipping [%s] - not a valid file", file.path().c_str());

        logger.Write(DEBUG, "Loaded vehicle config [%s]", config.Name.c_str());
    }
    logger.Write(INFO, "Configs loaded: %d", replays.size());

    return static_cast<unsigned>(replays.size());
}
