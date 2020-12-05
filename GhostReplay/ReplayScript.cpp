#include "ReplayScript.hpp"

#include "Compatibility.h"
#include "Constants.hpp"
#include "Util/Math.hpp"
#include "Util/Paths.hpp"
#include "Util/Game.hpp"
#include "Util/UI.hpp"
#include "Util/Logger.hpp"

#include <inc/enums.h>
#include <inc/natives.h>
#include <fmt/format.h>
#include <filesystem>
#include <algorithm>


using VExt = VehicleExtensions;

CReplayScript::CReplayScript(
    CScriptSettings& settings,
    std::vector<CReplayData>& replays,
    std::vector<CTrackData>& tracks)
    : mSettings(settings)
    , mReplays(replays)
    , mTracks(tracks)
    , mActiveReplay(nullptr)
    , mActiveTrack(nullptr)
    , mScriptMode(EScriptMode::Idle) {
}

CReplayScript::~CReplayScript() = default;

// Play usage
// So, this is how it works:
// 1. The player selects a track.
// 2. The player selects a ghost.
//      a. The ghost list is filtered by proximity of the ghost recording to the starting line.
// 3. The ghost replay starts as soon as the player vehicle passed the track.
//      a. The ghost replay stops (clears) when the player passes the finish.
//      b. The ghost replay restarts when the player passes the start again.
// 4. The current run is recorded in the background.
//      a. The run starts and stops recording data when passing the corresponding lines.
//      b. The run ignores < 5.0 second runs.
//      c. On a faster-than-recorded run gives the player choices. (Notify -> menu)
//          aa. Overwrite ghost | Save new ghost | Discard

// Record usage
// Track:
// 1. Record start line.
//      a. Save point A.
//      b. Save point B.
// 2. Record finish line.
//      a. Save point A.
//      b. Save point B.
//
// Replay:
// 1. A track is selected, but no ghost.
// 2. The player just does the run.
// 3. The run is recorded in the foreground.
//      a. The run starts and stops recording data when passing the corresponding lines.
//      b. The run ignores < 5.0 second runs.
//      c. On finishing a run, prompt the player explicitly. (Menu foreground)
//          aa. Save new ghost | Discard

void CReplayScript::Tick() {
    Vehicle playerVehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);

    switch(mScriptMode) {
        case EScriptMode::Idle:
            return;
        case EScriptMode::DefineTrack:
            updateTrackDefine();
            break;
        case EScriptMode::ReplayActive:
            updateReplay();
            break;
    }
}

void CReplayScript::SetTrack(const std::string& trackName) {
    for (auto& track : mTracks) {
        if (track.Name == trackName) {
            mActiveTrack = &track;
            return;
        }
    }
    logger.Write(ERROR, "SetTrack: No track found with the name [%s]", trackName.c_str());
}

void CReplayScript::SetReplay(const std::string& replayName) {
    for (auto& replay : mReplays) {
        if (replay.Name == replayName) {
            mActiveReplay = &replay;
            return;
        }
    }
    logger.Write(ERROR, "SetReplay: No replay found with the name [%s]", replayName.c_str());
}

void CReplayScript::updateReplay() {

}

void CReplayScript::updateTrackDefine() {

}
