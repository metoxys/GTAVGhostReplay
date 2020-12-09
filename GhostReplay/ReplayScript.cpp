#include "ReplayScript.hpp"
#include "Script.hpp"
#include "VehicleMod.h"

#include "Compatibility.h"
#include "Constants.hpp"
#include "Util/Math.hpp"
#include "Util/Paths.hpp"
#include "Util/Game.hpp"
#include "Util/UI.hpp"
#include "Util/Logger.hpp"
#include "Util/String.hpp"

#include <inc/enums.h>
#include <inc/natives.h>
#include <fmt/format.h>
#include <filesystem>
#include <algorithm>

#include "ReplayScriptUtils.hpp"

using VExt = VehicleExtensions;

CReplayScript::CReplayScript(
    CScriptSettings& settings,
    std::vector<CReplayData>& replays,
    std::vector<CTrackData>& tracks,
    std::vector<CImage>& trackImages)
    : mSettings(settings)
    , mReplays(replays)
    , mCompatibleReplays()
    , mTracks(tracks)
    , mTrackImages(trackImages)
    , mActiveReplay(nullptr)
    , mActiveTrack(nullptr)
    , mScriptMode(EScriptMode::ReplayActive)
    , mReplayState(EReplayState::Idle)
    , mRecordState(ERecordState::Idle)
    , mLastPos(Vector3{})
    , mReplayVehicle(0)
    , mCurrentRun(""){
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
    SetReplay("");
    mCompatibleReplays.clear();

    if (trackName.empty()) {
        mActiveTrack = nullptr;
        mReplayState = EReplayState::Idle;
        mRecordState = ERecordState::Idle;
        return;
    }

    for (auto& track : mTracks) {
        if (track.Name == trackName) {
            mActiveTrack = &track;
            mReplayState = EReplayState::Idle;
            mRecordState = ERecordState::Idle;
            mCompatibleReplays = GetCompatibleReplays(trackName);
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
        createReplayVehicle(0, nullptr, Vector3{});
        return;
    }

    for (auto& replay : mReplays) {
        if (replay.Name == replayName) {
            mActiveReplay = &replay;
            mLastNode = replay.Nodes.begin();
            mReplayState = EReplayState::Idle;
            mRecordState = ERecordState::Idle;
            createReplayVehicle(replay.VehicleModel, mActiveReplay, replay.Nodes[0].Pos);
            return;
        }
    }

    // createReplayVehicle should've notified already if we're here.
    if (mReplayVehicle == 0) {
        mActiveReplay = nullptr;
        mLastNode = std::vector<SReplayNode>::iterator();
        mReplayState = EReplayState::Idle;
        mRecordState = ERecordState::Idle;
        return;
    }
    logger.Write(ERROR, "SetReplay: No replay found with the name [%s]", replayName.c_str());
}

void CReplayScript::ClearUnsavedRuns() {
    mUnsavedRuns.clear();
}

std::vector<CReplayData>::const_iterator CReplayScript::EraseUnsavedRun(std::vector<CReplayData>::const_iterator runIt) {
    return mUnsavedRuns.erase(runIt);
}

std::vector<CReplayData> CReplayScript::GetCompatibleReplays(const std::string& trackName) {
    std::vector<CReplayData> replays;
    for (auto& replay : mReplays) {
        if (replay.Track == trackName)
            replays.push_back(replay);
    }
    return replays;
}

void CReplayScript::AddCompatibleReplay(const CReplayData& value) {
    mCompatibleReplays.push_back(value);
}

bool CReplayScript::IsFastestLap(const std::string& trackName, Hash vehicleModel, unsigned long long timestamp) {
    CReplayData fastestReplay = GetFastestReplay(trackName, vehicleModel);

    if (!fastestReplay.Name.empty())
        return timestamp < fastestReplay.Nodes.back().Timestamp;

    return false;
}

CReplayData CReplayScript::GetFastestReplay(const std::string& trackName, Hash vehicleModel) {
    CReplayData fastestReplay("");

    for (auto& replay : mReplays) {
        if (replay.Track != trackName)
            continue;

        if (vehicleModel != 0 && replay.VehicleModel != vehicleModel)
            continue;

        if (fastestReplay.Nodes.empty() ||
            replay.Nodes.back().Timestamp < fastestReplay.Nodes.back().Timestamp) {
            fastestReplay = replay;
        }
    }
    return fastestReplay;
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

void CReplayScript::DeleteTrack(const CTrackData& track) {
    auto trackIt = std::find_if(mTracks.begin(), mTracks.end(), [&](const CTrackData& x) {
        return x.FileName() == track.FileName();
    });

    if (trackIt == mTracks.end()) {
        logger.Write(ERROR, "[Track] Attempted to delete track [%s] but didn't find it in the list? Filename: [%s]",
            track.Name.c_str(), track.FileName().c_str());
        UI::Notify(fmt::format("[Track] Failed to delete {}", track.Name));
        return;
    }

    if (mActiveTrack) {
        if (mActiveTrack->FileName() == track.FileName()) {
            SetTrack("");
        }
    }
    track.Delete();
    mTracks.erase(trackIt);
    logger.Write(INFO, "[Track] Deleted track [%s], Filename: [%s]",
        track.Name.c_str(), track.FileName().c_str());
}

void CReplayScript::DeleteReplay(const CReplayData& replay) {
    auto replayIt = std::find_if(mReplays.begin(), mReplays.end(), [&](const CReplayData& x) {
        return x.FileName() == replay.FileName();
    });
    // Because it's a copy, let's just update this in parallel.
    auto replayCompIt = std::find_if(mCompatibleReplays.begin(), mCompatibleReplays.end(), [&](const CReplayData& x) {
        return x.FileName() == replay.FileName();
    });

    if (replayIt == mReplays.end()) {
        logger.Write(ERROR, "[Replay] Attempted to delete replay [%s] but didn't find it in the list? Filename: [%s]",
            replay.Name.c_str(), replay.FileName().c_str());
        UI::Notify(fmt::format("[Replay] Failed to delete {}", replay.Name));
        return;
    }

    if (replayCompIt == mCompatibleReplays.end()) {
        logger.Write(ERROR, "[Replay] Attempted to delete replay [%s] but didn't find it in the compatible list? Filename: [%s]",
            replay.Name.c_str(), replay.FileName().c_str());
        UI::Notify(fmt::format("[Replay] Failed to delete {}", replay.Name));
        return;
    }

    if (mActiveReplay) {
        if (mActiveReplay->FileName() == replay.FileName()) {
            SetReplay("");
        }
    }
    replay.Delete();
    mReplays.erase(replayIt);
    mCompatibleReplays.erase(replayCompIt);
    logger.Write(INFO, "[Replay] Deleted replay [%s], Filename: [%s]",
        replay.Name.c_str(), replay.FileName().c_str());
}

std::string CReplayScript::GetTrackImageMenuString(const std::string& trackName) {
    std::string extra;

    auto imgIt = std::find_if(mTrackImages.begin(), mTrackImages.end(), [trackName](const CImage& img) {
        return img.Name == trackName;
    });

    if (imgIt != mTrackImages.end()) {
        extra = fmt::format("{}W{}H{}",
            imgIt->TextureID,
            imgIt->ResX,
            imgIt->ResY);
    }
    return extra;
}

void CReplayScript::updateReplay() {
    if (!mActiveTrack)
        return;

    Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);
    Vector3 nowPos;
    if (ENTITY::DOES_ENTITY_EXIST(vehicle)) {
        nowPos = ENTITY::GET_ENTITY_COORDS(vehicle, true);
    }
    else {
        nowPos = mLastPos;
    }

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

    updatePlayback(gameTime, startPassedThisTick, finishPassedThisTick);
    updateRecord(gameTime, startPassedThisTick, finishPassedThisTick);
}

void CReplayScript::updatePlayback(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick) {
    switch (mReplayState) {
        case EReplayState::Idle: {
            if (startPassedThisTick) {
                mReplayState = EReplayState::Playing;
                replayStart = gameTime;
                ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, true, true);
                ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, mSettings.Replay.VehicleAlpha, false);
                ENTITY::SET_ENTITY_COLLISION(mReplayVehicle, false, false);
                VEHICLE::SET_VEHICLE_ENGINE_ON(mReplayVehicle, true, true, false);
                //UI::Notify("Replay started", false);
            }
            break;
        }
        case EReplayState::Playing: {
            if (!mActiveReplay)
                break;
            auto replayTime = gameTime - replayStart;
            auto nodeCurr = std::upper_bound(mLastNode, mActiveReplay->Nodes.end(), SReplayNode{ replayTime });
            if (nodeCurr != mActiveReplay->Nodes.begin())
                nodeCurr = std::prev(nodeCurr);

            mLastNode = nodeCurr;

            if (nodeCurr == mActiveReplay->Nodes.end()) {
                mReplayState = EReplayState::Finished;
                //UI::Notify("Replay finished", false);
                break;
            }

            auto nodeNext = nodeCurr == std::prev(mActiveReplay->Nodes.end()) ? nodeCurr : std::next(nodeCurr);

            float progress = ((float)replayTime - (float)nodeCurr->Timestamp) / 
                ((float)nodeNext->Timestamp - (float)nodeCurr->Timestamp);

            Vector3 pos = vlerp(nodeCurr->Pos, nodeNext->Pos, progress);
            Vector3 rot = nodeCurr->Rot;
            ENTITY::SET_ENTITY_COORDS(mReplayVehicle, pos.x, pos.y, pos.z, false, false, false, false);
            ENTITY::SET_ENTITY_ROTATION(mReplayVehicle, rot.x, rot.y, rot.z, 0, false);

            if (VExt::GetNumWheels(mReplayVehicle) == nodeCurr->WheelRotations.size()) {
                for (uint8_t idx = 0; idx < VExt::GetNumWheels(mReplayVehicle); ++idx) {
                    VExt::SetWheelRotation(mReplayVehicle, idx, nodeCurr->WheelRotations[idx]);
                }
            }

            VExt::SetSteeringAngle(mReplayVehicle, nodeCurr->SteeringAngle);
            VExt::SetThrottle(mReplayVehicle, nodeCurr->Throttle);
            VExt::SetThrottleP(mReplayVehicle, nodeCurr->Throttle);
            VExt::SetBrakeP(mReplayVehicle, nodeCurr->Brake);

            if (nodeCurr->Gear >= 0 && nodeCurr->RPM > 0.0f) {
                VExt::SetGearCurr(mReplayVehicle, static_cast<uint16_t>(nodeCurr->Gear));
                VExt::SetGearNext(mReplayVehicle, static_cast<uint16_t>(nodeCurr->Gear));
                VExt::SetCurrentRPM(mReplayVehicle, nodeCurr->RPM);
            }

            VEHICLE::SET_VEHICLE_BRAKE_LIGHTS(mReplayVehicle, nodeCurr->Brake > 0.1f);

            VEHICLE::SET_VEHICLE_LIGHTS(mReplayVehicle, nodeCurr->LowBeams ? 3 : 4);
            VEHICLE::SET_VEHICLE_FULLBEAM(mReplayVehicle, nodeCurr->HighBeams);

            if (finishPassedThisTick) {
                mReplayState = EReplayState::Finished;
                //UI::Notify("Player finished", false);
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
}

void CReplayScript::updateRecord(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick) {
    Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);
    if (!ENTITY::DOES_ENTITY_EXIST(vehicle) || !mActiveTrack) {
        mRecordState = ERecordState::Idle;
        mCurrentRun = CReplayData("");
        return;
    }

    Vector3 nowPos = ENTITY::GET_ENTITY_COORDS(vehicle, true);
    Vector3 nowRot = ENTITY::GET_ENTITY_ROTATION(vehicle, 0);

    switch (mRecordState) {
        case ERecordState::Idle: {
            if (startPassedThisTick) {
                mRecordState = ERecordState::Recording;
                recordStart = gameTime;
                mCurrentRun.Track = mActiveTrack->Name;
                mCurrentRun.Nodes.clear();
                mCurrentRun.VehicleModel = ENTITY::GET_ENTITY_MODEL(vehicle);

                VehicleModData modData = VehicleModData::LoadFrom(vehicle);
                mCurrentRun.VehicleMods = modData;
                //UI::Notify("Record started", false);
            }
            break;
        }
        case ERecordState::Recording: {
            SReplayNode node{};
            node.Timestamp = gameTime - recordStart;
            node.Pos = nowPos;
            node.Rot = nowRot;
            node.WheelRotations = VExt::GetWheelRotations(vehicle);

            node.SteeringAngle = VExt::GetSteeringAngle(vehicle);
            node.Throttle = VExt::GetThrottleP(vehicle);
            node.Brake = VExt::GetBrakeP(vehicle);

            node.Gear = VExt::GetGearCurr(vehicle);
            node.RPM = VExt::GetCurrentRPM(vehicle);

            BOOL areLowBeamsOn, areHighBeamsOn;
            VEHICLE::GET_VEHICLE_LIGHTS_STATE(vehicle, &areLowBeamsOn, &areHighBeamsOn);
            node.LowBeams = areLowBeamsOn;
            node.HighBeams = areHighBeamsOn;

            bool saved = false;
            unsigned long long lastNodeTime = 0;
            if (!mCurrentRun.Nodes.empty()) {
                lastNodeTime = mCurrentRun.Nodes.back().Timestamp;
            }
            if (node.Timestamp >= lastNodeTime + mSettings.Record.DeltaMillis) {
                mCurrentRun.Nodes.push_back(node);
                saved = true;
            }

            if (finishPassedThisTick) {
                mRecordState = ERecordState::Finished;
                if (!saved) {
                    mCurrentRun.Nodes.push_back(node);
                }

                bool fastestLap = false;
                bool fasterLap = false;

                if (mActiveReplay) {
                    fasterLap = node.Timestamp < mActiveReplay->Nodes.back().Timestamp;
                }
                else {
                    fasterLap = true;
                }
                if (fasterLap) {
                    fastestLap = IsFastestLap(mCurrentRun.Track, 0, node.Timestamp);
                }

                std::string lapInfo;

                if (mSettings.Record.AutoGhost && (!mActiveReplay || fasterLap)) {
                    mCurrentRun.Name = Util::FormatReplayName(
                        node.Timestamp,
                        mCurrentRun.Track,
                        Util::GetVehicleName(mCurrentRun.VehicleModel));
                    CReplayData::WriteAsync(mCurrentRun);
                    GhostReplay::AddReplay(mCurrentRun);
                    mCompatibleReplays.push_back(mCurrentRun);
                    SetReplay(mCurrentRun.Name);
                }
                else {
                    mUnsavedRuns.push_back(mCurrentRun);
                }

                lapInfo = fmt::format("Lap time: {}{}",
                    fastestLap ? "~p~" : fasterLap ? "~g~" : "~y~",
                    Util::FormatMillisTime(node.Timestamp));

                // TODO: Movable UI element with current time & laptime & delta?

                if (mSettings.Main.NotifyLaps) {
                    UI::Notify(lapInfo, false);
                }
            }
            break;
        }
        case ERecordState::Finished: {
            mRecordState = ERecordState::Idle;
            mCurrentRun = CReplayData("");
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

void CReplayScript::createReplayVehicle(Hash model, CReplayData* activeReplay, Vector3 pos) {
    if (ENTITY::DOES_ENTITY_EXIST(mReplayVehicle)) {
        ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, false, false);
        ENTITY::DELETE_ENTITY(&mReplayVehicle);
        mReplayVehicle = 0;
    }
    if (model == 0 || activeReplay == nullptr) {
        return;
    }

    const char* fallbackModelName = mSettings.Replay.FallbackModel.c_str();

    if (mSettings.Replay.ForceFallbackModel)
        model = MISC::GET_HASH_KEY(fallbackModelName);

    if (!(STREAMING::IS_MODEL_IN_CDIMAGE(model) && STREAMING::IS_MODEL_A_VEHICLE(model))) {
        // Vehicle doesn't exist
        std::string msg =
            fmt::format("Error: Couldn't find model 0x{:08X}. Falling back to ({}).", model, fallbackModelName);
        UI::Notify(msg, false);
        logger.Write(ERROR, fmt::format("[Replay] {}", msg));

        model = MISC::GET_HASH_KEY(fallbackModelName);
        if (!(STREAMING::IS_MODEL_IN_CDIMAGE(model) && STREAMING::IS_MODEL_A_VEHICLE(model))) {
            msg = "Error: Failed to find fallback model.";
            UI::Notify(msg, false);
            logger.Write(ERROR, fmt::format("[Replay] {}", msg));
            return;
        }
    }
    STREAMING::REQUEST_MODEL(model);
    auto startTime = GetTickCount64();

    while (!STREAMING::HAS_MODEL_LOADED(model)) {
        WAIT(0);
        if (GetTickCount64() > startTime + 5000) {
            // Couldn't load model
            WAIT(0);
            STREAMING::SET_MODEL_AS_NO_LONGER_NEEDED(model);
            std::string msg = fmt::format("Error: Failed to load model 0x{:08X}.", model);
            UI::Notify(msg, false);
            logger.Write(ERROR, fmt::format("[Replay] {}", msg));
            return;
        }
    }

    mReplayVehicle = VEHICLE::CREATE_VEHICLE(model,
        pos.x, pos.y, pos.z, 0, false, true, false);

    ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, false, false);
    ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, 0, false);
    ENTITY::SET_ENTITY_COLLISION(mReplayVehicle, false, false);

    VehicleModData modData = mActiveReplay->VehicleMods;
    VehicleModData::ApplyTo(mReplayVehicle, modData);
}
