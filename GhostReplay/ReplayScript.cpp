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
    , mScriptMode(EScriptMode::ReplayActive)
    , mReplayState(EReplayState::Idle)
    , mRecordState(ERecordState::Idle)
    , mLastPos(Vector3{})
    , mReplayVehicle(0)
    , mCurrentRun(){
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
    switch(mScriptMode) {
        case EScriptMode::DefineTrack:
            updateTrackDefine();
            break;
        case EScriptMode::ReplayActive:
            updateReplay();
            break;
    }
}

void CReplayScript::SetTrack(const std::string& trackName) {
    if (trackName.empty()) {
        mActiveTrack = nullptr;
        mReplayState = EReplayState::Idle;
        mRecordState = ERecordState::Idle;
        return;
    }

    for (auto& track : mTracks) {
        if (track.Name == trackName) {
            createReplayVehicle(0, Vector3{});
            mActiveTrack = &track;
            mReplayState = EReplayState::Idle;
            mRecordState = ERecordState::Idle;
            return;
        }
    }
    logger.Write(ERROR, "SetTrack: No track found with the name [%s]", trackName.c_str());
}

void CReplayScript::SetReplay(const std::string& replayName) {
    if (replayName.empty()) {
        mActiveReplay = nullptr;
        mLastNode = std::vector<SReplayNode>::iterator();
        mReplayState = EReplayState::Idle;
        mRecordState = ERecordState::Idle;
        return;
    }

    for (auto& replay : mReplays) {
        if (replay.Name == replayName) {
            createReplayVehicle(replay.VehicleModel, replay.Nodes[0].Pos);
            mActiveReplay = &replay;
            mLastNode = replay.Nodes.begin();
            mReplayState = EReplayState::Idle;
            mRecordState = ERecordState::Idle;
            return;
        }
    }
    logger.Write(ERROR, "SetReplay: No replay found with the name [%s]", replayName.c_str());
}

void CReplayScript::ClearUnsavedRuns() {
    mUnsavedRuns.clear();
}

std::vector<CReplayData>::const_iterator CReplayScript::EraseUnsavedRun(std::vector<CReplayData>::const_iterator runIt) {
    return mUnsavedRuns.erase(runIt);
}

bool CReplayScript::StartLineDef(SLineDef& lineDef, const std::string& lineName) {
    const std::string escapeKey = "BACKSPACE";

    int progress = 0;

    while(true) {
        if (PAD::IS_CONTROL_JUST_RELEASED(0, eControl::ControlFrontendCancel)) {
            return false;
        }

        auto coords = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), true);

        UI::DrawSphere(coords, 0.25f, 255, 255, 255, 255);

        if (progress == 1) {
            UI::DrawSphere(lineDef.A, 0.25f, 255, 255, 255, 255);
            UI::DrawLine(lineDef.A, coords, 255, 255, 255, 255);
        }

        if (PAD::IS_CONTROL_JUST_RELEASED(0, eControl::ControlVehicleHorn)) {
            switch (progress) {
                case 0: {
                    lineDef.A = coords;
                    progress++;
                    break;
                }
                case 1: {
                    lineDef.B = coords;
                    return true;
                }
                default: {
                    return false;
                }
            }
        }

        std::string currentStepName = fmt::format("{} {} point", progress == 0 ? "left" : "right", lineName);

        UI::ShowHelpText(fmt::format(
            "Press [Horn] to register {} at your current location. "
            "Press [{}] to cancel and exit.", 
            currentStepName, escapeKey));
        WAIT(0);
    }
}

void CReplayScript::updateReplay() {
    Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);
    if (!ENTITY::DOES_ENTITY_EXIST(vehicle) || !mActiveTrack)
        return;

    Vector3 nowPos = ENTITY::GET_ENTITY_COORDS(vehicle, true);
    Vector3 nowRot = ENTITY::GET_ENTITY_ROTATION(vehicle, 0);

    unsigned long long gameTime = MISC::GET_GAME_TIMER();
    bool startPassedThisTick = passedLineThisTick(mActiveTrack->StartLine, mLastPos, nowPos);
    bool finishPassedThisTick = passedLineThisTick(mActiveTrack->FinishLine, mLastPos, nowPos);

    // Prevents default coords from activating record/replay
    if (mLastPos == Vector3{} || startPassedThisTick && finishPassedThisTick) {
        startPassedThisTick = false;
        finishPassedThisTick = false;
    }

    mLastPos = nowPos;

    Vector3 emptyVec{};
    if (mActiveTrack->StartLine.A != emptyVec &&
        mActiveTrack->StartLine.B != emptyVec) {
        UI::DrawSphere(mActiveTrack->StartLine.A, 0.25f, 255, 255, 255, 255);
        UI::DrawSphere(mActiveTrack->StartLine.B, 0.25f, 255, 255, 255, 255);
        UI::DrawLine(mActiveTrack->StartLine.A, mActiveTrack->StartLine.B, 255, 255, 255, 255);
    }

    if (mActiveTrack->FinishLine.A != emptyVec &&
        mActiveTrack->FinishLine.B != emptyVec) {
        UI::DrawSphere(mActiveTrack->FinishLine.A, 0.25f, 255, 255, 255, 255);
        UI::DrawSphere(mActiveTrack->FinishLine.B, 0.25f, 255, 255, 255, 255);
        UI::DrawLine(mActiveTrack->FinishLine.A, mActiveTrack->FinishLine.B, 255, 255, 255, 255);
    }

    switch (mReplayState) {
        case EReplayState::Idle: {
            if (startPassedThisTick) {
                mReplayState = EReplayState::Playing;
                replayStart = gameTime;
                ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, true, true);
                ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, 127, false);
                ENTITY::SET_ENTITY_COLLISION(mReplayVehicle, false, false);
                UI::Notify("Replay started", false);
            }
            break;
        }
        case EReplayState::Playing: {
            if (!mActiveReplay)
                break;
            auto replayTime = gameTime - replayStart;
            auto nodeCurr = std::find_if(mLastNode, mActiveReplay->Nodes.end(), [replayTime](const SReplayNode& node) {
                return node.Timestamp >= replayTime;
            });
            mLastNode = nodeCurr;

            if (nodeCurr == mActiveReplay->Nodes.end()) {
                mReplayState = EReplayState::Finished;
                UI::Notify("Replay finished", false);
                break;
            }

            auto nodeNext = nodeCurr == std::prev(mActiveReplay->Nodes.end()) ? nodeCurr : std::next(nodeCurr);

            float progress = ((float)replayTime - (float)nodeCurr->Timestamp) / ((float)nodeNext->Timestamp - (float)nodeCurr->Timestamp);

            Vector3 pos = vlerp(nodeCurr->Pos, nodeNext->Pos, progress);
            Vector3 rot = nodeCurr->Rot;
            ENTITY::SET_ENTITY_COORDS(mReplayVehicle, pos.x, pos.y, pos.z, false, false, false, false);
            ENTITY::SET_ENTITY_ROTATION(mReplayVehicle, rot.x, rot.y, rot.z, 0, false);

            if (finishPassedThisTick) {
                mReplayState = EReplayState::Finished;
                UI::Notify("Player finished", false);
            }
            break;
        }
        case EReplayState::Finished: {
            mReplayState = EReplayState::Idle;
            ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, false, false);
            mLastNode = mActiveReplay->Nodes.begin();
            break;
        }
    }

    switch (mRecordState) {
        case ERecordState::Idle: {
            if (startPassedThisTick) {
                mRecordState = ERecordState::Recording;
                recordStart = gameTime;
                mCurrentRun.Track = mActiveTrack->Name;
                mCurrentRun.Nodes.clear();
                mCurrentRun.VehicleModel = ENTITY::GET_ENTITY_MODEL(vehicle);
                UI::Notify("Record started", false);
            }
            break;
        }
        case ERecordState::Recording: {
            SReplayNode node{};
            node.Timestamp = gameTime - recordStart;
            node.Pos = nowPos;
            node.Rot = nowRot;
            mCurrentRun.Nodes.push_back(node);

            if (finishPassedThisTick) {
                mRecordState = ERecordState::Finished;
                mUnsavedRuns.push_back(mCurrentRun);
                UI::Notify("Record stopped", false);
            }
            break;
        }
        case ERecordState::Finished: {
            mRecordState = ERecordState::Idle;
            break;
        }
    }
}

void CReplayScript::updateTrackDefine() {

}

bool CReplayScript::passedLineThisTick(SLineDef line, Vector3 oldPos, Vector3 newPos) {
    if (Distance(line.A, newPos) > Distance(line.A, line.B) || Distance(line.B, newPos) > Distance(line.A, line.B)) {
        return false;
    }
    float oldSgn = sgn(GetAngleABC(line.A, line.B, oldPos));
    float newSgn = sgn(GetAngleABC(line.A, line.B, newPos));
    return oldSgn != newSgn && oldSgn > 0.0f;
}

void CReplayScript::createReplayVehicle(Hash model, Vector3 pos) {
    if (ENTITY::DOES_ENTITY_EXIST(mReplayVehicle)) {
        ENTITY::DELETE_ENTITY(&mReplayVehicle);
        mReplayVehicle = 0;
    }
    if (model == 0) {
        return;
    }

    mReplayVehicle = VEHICLE::CREATE_VEHICLE(model,
        pos.x, pos.y, pos.z, 0, false, true, false);

    ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, false, false);
    ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, 0, false);
    ENTITY::SET_ENTITY_COLLISION(mReplayVehicle, false, false);
}
