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
    std::vector<CTrackData> tracks;
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
    GhostReplay::LoadTracks();

    scriptInst = std::make_shared<CReplayScript>(*settings, replays, tracks);

    VehicleExtensions::Init();
    Compatibility::Setup();

    CScriptMenu menu(settingsMenuPath, 
        []() {
            // OnInit
            settings->Load();
            GhostReplay::LoadReplays();
            GhostReplay::LoadTracks();
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
    const std::string replaysPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\Replays";

    logger.Write(DEBUG, "Clearing and reloading replays");

    replays.clear();

    if (!(fs::exists(fs::path(replaysPath)) && fs::is_directory(fs::path(replaysPath)))) {
        logger.Write(ERROR, "Directory [%s] not found!", replaysPath.c_str());
        return 0;
    }

    for (const auto& file : fs::directory_iterator(replaysPath)) {
        if (Util::to_lower(fs::path(file).extension().string()) != ".json") {
            logger.Write(DEBUG, "Skipping [%s] - not .json", file.path().c_str());
            continue;
        }

        CReplayData replay = CReplayData::Read(fs::path(file).string());
        if (!replay.Nodes.empty())
            replays.push_back(replay);
        else
            logger.Write(WARN, "Skipping [%s] - not a valid file", file.path().c_str());

        logger.Write(DEBUG, "Loaded replay [%s]", replay.Name.c_str());
    }
    logger.Write(INFO, "Replays loaded: %d", replays.size());

    return static_cast<unsigned>(replays.size());
}

uint32_t GhostReplay::LoadTracks() {
    const std::string tracksPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\Tracks";

    logger.Write(DEBUG, "Clearing and reloading tracks");

    tracks.clear();

    if (!(fs::exists(fs::path(tracksPath)) && fs::is_directory(fs::path(tracksPath)))) {
        logger.Write(ERROR, "Directory [%s] not found!", tracksPath.c_str());
        return 0;
    }

    for (const auto& file : fs::directory_iterator(tracksPath)) {
        if (Util::to_lower(fs::path(file).extension().string()) != ".json") {
            logger.Write(DEBUG, "Skipping [%s] - not .json", file.path().c_str());
            continue;
        }

        CTrackData track = CTrackData::Read(fs::path(file).string());
        if (!track.Name.empty())
            tracks.push_back(track);
        else
            logger.Write(WARN, "Skipping [%s] - not a valid file", file.path().c_str());

        logger.Write(DEBUG, "Loaded track [%s]", track.Name.c_str());
    }
    logger.Write(INFO, "Tracks loaded: %d", tracks.size());

    return static_cast<unsigned>(tracks.size());
}