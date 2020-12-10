#include "Script.hpp"

#include "ReplayScript.hpp"
#include "ScriptMenu.hpp"
#include "Constants.hpp"
#include "Compatibility.h"
#include "Memory/VehicleExtensions.hpp"

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
    std::vector<CTrackData> arsTracks;
    std::vector<CImage> trackImages;

    void clearFileFlags() {
        for (auto& track : tracks) {
            track.MarkedForDeletion = false;
        }

        for (auto& replay : replays) {
            replay.MarkedForDeletion = false;
        }
    }
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
    GhostReplay::LoadARSTracks();
    GhostReplay::LoadTrackImages();

    scriptInst = std::make_shared<CReplayScript>(*settings, replays, tracks, trackImages, arsTracks);

    VehicleExtensions::Init();
    Compatibility::Setup();

    CScriptMenu menu(settingsMenuPath, 
        []() {
            // OnInit
            settings->Load();
            // GhostReplay::LoadReplays();
            // GhostReplay::LoadTracks();
        },
        []() {
            // OnExit
            scriptInst->SetScriptMode(EScriptMode::ReplayActive);
            settings->Save();
            ::clearFileFlags();
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

uint32_t GhostReplay::LoadReplays() {
    const std::string replaysPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\Replays";

    logger.Write(DEBUG, "[Replay] Clearing and reloading replays");

    replays.clear();

    if (!(fs::exists(fs::path(replaysPath)) && fs::is_directory(fs::path(replaysPath)))) {
        logger.Write(ERROR, "[Replay] Directory [%s] not found!", replaysPath.c_str());
        return 0;
    }

    for (const auto& file : fs::directory_iterator(replaysPath)) {
        if (Util::to_lower(fs::path(file).extension().string()) != ".json") {
            logger.Write(DEBUG, "[Replay] Skipping [%s] - not .json", fs::path(file).string().c_str());
            continue;
        }

        CReplayData replay = CReplayData::Read(fs::path(file).string());
        if (!replay.Nodes.empty())
            replays.push_back(replay);
        else
            logger.Write(WARN, "[Replay] Skipping [%s] - not a valid file", fs::path(file).string().c_str());

        logger.Write(DEBUG, "[Replay] Loaded replay [%s]", replay.Name.c_str());
    }
    logger.Write(INFO, "[Replay] Replays loaded: %d", replays.size());

    return static_cast<unsigned>(replays.size());
}

uint32_t GhostReplay::LoadTracks() {
    const std::string tracksPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\Tracks";

    logger.Write(DEBUG, "[Track] Clearing and reloading tracks");

    tracks.clear();

    if (!(fs::exists(fs::path(tracksPath)) && fs::is_directory(fs::path(tracksPath)))) {
        logger.Write(ERROR, "[Track] Directory [%s] not found!", tracksPath.c_str());
        return 0;
    }

    for (const auto& file : fs::recursive_directory_iterator(tracksPath)) {
        if (Util::to_lower(fs::path(file).extension().string()) != ".json") {
            logger.Write(DEBUG, "[Track] Skipping [%s] - not .json", fs::path(file).string().c_str());
            continue;
        }

        CTrackData track = CTrackData::Read(fs::path(file).string());
        if (!track.Name.empty())
            tracks.push_back(track);
        else
            logger.Write(WARN, "[Track] Skipping [%s] - not a valid file", fs::path(file).string().c_str());

        logger.Write(DEBUG, "[Track] Loaded track [%s]", track.Name.c_str());
    }
    logger.Write(INFO, "[Track] Tracks loaded: %d", tracks.size());

    return static_cast<unsigned>(tracks.size());
}

uint32_t GhostReplay::LoadTrackImages() {
    const std::string tracksPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\Tracks";

    logger.Write(DEBUG, "[TrackImg] Clearing and reloading track images");

    trackImages.clear();

    if (!(fs::exists(fs::path(tracksPath)) && fs::is_directory(fs::path(tracksPath)))) {
        logger.Write(ERROR, "[TrackImg] Directory [%s] not found!", tracksPath.c_str());
        return 0;
    }

    for (const auto& file : fs::recursive_directory_iterator(tracksPath)) {
        auto stem = fs::path(file).stem().string();
        auto okExts = Img::GetAllowedExtensions();
        auto ext = Util::to_lower(fs::path(file).extension().string());
        if (std::find(okExts.begin(), okExts.end(), ext) == okExts.end()) {
            logger.Write(DEBUG, "[TrackImg] Skipping [%s] - not .png, .jpg or .jpeg", fs::path(file).string().c_str());
            continue;
        }

        std::string fileName = fs::path(file).string();
        unsigned width;
        unsigned height;
        if (!Img::GetIMGDimensions(fileName, &width, &height)) {
            logger.Write(WARN, "[TrackImg] [%s] Couldn't determine image size, using default 480x270.",
                fs::path(file).string().c_str());
            width = 480;
            height = 270;
        }
        int handle = createTexture(fileName.c_str());
        trackImages.emplace_back(handle, stem, width, height);

        logger.Write(DEBUG, "[TrackImg] Loaded track image [%s]", stem.c_str());
    }
    logger.Write(INFO, "[TrackImg] Track images loaded: %d", trackImages.size());

    return static_cast<unsigned>(trackImages.size());
}

uint32_t GhostReplay::LoadARSTracks() {
    const std::string tracksPath =
        Paths::GetRunningExecutableFolder() + "\\Scripts\\ARS\\Tracks";

    logger.Write(DEBUG, "[Track-ARS] Clearing and reloading tracks");

    arsTracks.clear();

    if (!(fs::exists(fs::path(tracksPath)) && fs::is_directory(fs::path(tracksPath)))) {
        logger.Write(ERROR, "[Track-ARS] Directory [%s] not found!", tracksPath.c_str());
        return 0;
    }

    for (const auto& file : fs::recursive_directory_iterator(tracksPath)) {
        if (Util::to_lower(fs::path(file).extension().string()) != ".xml") {
            logger.Write(DEBUG, "[Track-ARS] Skipping [%s] - not .xml", fs::path(file).string().c_str());
            continue;
        }

        CTrackData track = CTrackData::ReadARS(fs::path(file).string());
        if (!track.Name.empty()) {
            tracks.push_back(track);
            logger.Write(DEBUG, "[Track-ARS] Loaded track [%s]", track.Name.c_str());
        }
        else {
            logger.Write(WARN, "[Track-ARS] Skipping [%s] - not a valid file", fs::path(file).string().c_str());
        }
    }
    logger.Write(INFO, "[Track-ARS] Tracks loaded: %d", tracks.size());

    return static_cast<unsigned>(tracks.size());
}

void GhostReplay::AddReplay(CReplayData replay) {
    replays.push_back(replay);
}
