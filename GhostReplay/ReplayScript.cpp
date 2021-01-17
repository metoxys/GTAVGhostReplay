#include "ReplayScript.hpp"
#include "Script.hpp"
#include "VehicleMod.h"

#include "Compatibility.h"
#include "Constants.hpp"
#include "ReplayScriptUtils.hpp"

#include "Memory/VehicleExtensions.hpp"
#include "Util/Math.hpp"
#include "Util/Paths.hpp"
#include "Util/Game.hpp"
#include "Util/UI.hpp"
#include "Util/Logger.hpp"
#include "Util/String.hpp"
#include "Util/Inputs.hpp"

#include <inc/enums.h>
#include <inc/natives.h>
#include <fmt/format.h>
#include <filesystem>
#include <algorithm>

using VExt = VehicleExtensions;

CReplayScript::CReplayScript(
    CScriptSettings& settings,
    std::vector<CReplayData>& replays,
    std::vector<CTrackData>& tracks,
    std::vector<CImage>& trackImages,
    std::vector<CTrackData>& arsTracks)
    : mSettings(settings)
    , mReplays(replays)
    , mCompatibleReplays()
    , mTracks(tracks)
    , mTrackImages(trackImages)
    , mArsTracks(arsTracks)
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
    clearPtfx();

    if (trackName.empty()) {
        mActiveTrack = nullptr;
        mReplayState = EReplayState::Idle;
        mRecordState = ERecordState::Idle;
        return;
    }

    CTrackData* foundTrack = nullptr;
    for (auto& track : mTracks) {
        if (track.Name == trackName) {
            foundTrack = &track;
            break;
        }
    }

    for (auto& track : mArsTracks) {
        if (track.Name == trackName) {
            foundTrack = &track;
            break;
        }
    }

    if (foundTrack != nullptr) {
        mActiveTrack = foundTrack;
        mReplayState = EReplayState::Idle;
        mRecordState = ERecordState::Idle;
        mCompatibleReplays = GetCompatibleReplays(trackName);
        createPtfx(*foundTrack);

        if (mSettings.Replay.AutoLoadGhost) {
            Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);
            auto fastestReplay = GetFastestReplay(trackName, ENTITY::GET_ENTITY_MODEL(vehicle));
            if (!fastestReplay.Name.empty() && ENTITY::DOES_ENTITY_EXIST(vehicle)) {
                SetReplay(fastestReplay.Name, fastestReplay.Timestamp);
            }
        }
        return;
    }

    logger.Write(ERROR, "SetTrack: No track found with the name [%s]", trackName.c_str());
}

void CReplayScript::SetReplay(const std::string& replayName, unsigned long long timestamp) {
    if (replayName.empty()) {
        mActiveReplay = nullptr;
        mLastNode = std::vector<SReplayNode>::iterator();
        mReplayState = EReplayState::Idle;
        mRecordState = ERecordState::Idle;
        createReplayVehicle(0, nullptr, Vector3{});
        return;
    }

    for (auto& replay : mReplays) {
        bool nameOK = replay.Name == replayName;
        bool timeOK = timestamp == 0 ? true : replay.Timestamp == timestamp;

        if (nameOK && timeOK) {
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
    int progress = 0;

    const eControl cancelControl = eControl::ControlFrontendCancel;
    const eControl registerControl = eControl::ControlContext;

    createPtfx(*mActiveTrack);

    bool result;
    while(true) {
        if (PAD::IS_CONTROL_JUST_RELEASED(0, cancelControl)) {
            result = false;
            goto exit;
        }

        auto coords = ENTITY::GET_ENTITY_COORDS(PLAYER::PLAYER_PED_ID(), true);

        UI::DrawSphere(coords, 0.20f, 255, 255, 255, 255);

        if (progress == 1) {
            UI::DrawSphere(lineDef.A, 0.25f, 255, 255, 255, 255);
            UI::DrawLine(lineDef.A, coords, 255, 255, 255, 255);
        }

        if (PAD::IS_CONTROL_JUST_RELEASED(0, registerControl)) {
            switch (progress) {
                case 0: {
                    lineDef.A = coords;
                    progress++;
                    break;
                }
                case 1: {
                    lineDef.B = coords;
                    result = true;
                    goto exit;
                }
                default: {
                    result = false;
                    goto exit;
                }
            }
        }

        std::string currentStepName = fmt::format("{} {} point", progress == 0 ? "left" : "right", lineName);

        UI::ShowHelpText(fmt::format(
            "Press {} to register {} at your current location. "
            "Press {} to cancel and exit.", 
            Inputs::GetControlString(registerControl), currentStepName,
            Inputs::GetControlString(cancelControl)));
        WAIT(0);
    }

exit:
    clearPtfx();
    return result;
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
    if (mLastPos == Vector3{}) {
        startPassedThisTick = false;
        finishPassedThisTick = false;
    }

    mLastPos = nowPos;

    updatePlayback(gameTime, startPassedThisTick, finishPassedThisTick);
    updateRecord(gameTime, startPassedThisTick, finishPassedThisTick);
}

void CReplayScript::updatePlayback(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick) {
    if (!mActiveReplay) {
        mReplayState = EReplayState::Idle;
        return;
    }

    switch (mReplayState) {
        case EReplayState::Idle: {
            if (startPassedThisTick) {
                mReplayState = EReplayState::Playing;
                replayStart = gameTime;
                ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, true, true);
                ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, map(mSettings.Replay.VehicleAlpha, 0, 100, 0, 255), false);
                ENTITY::SET_ENTITY_COMPLETELY_DISABLE_COLLISION(mReplayVehicle, false, false);
                VEHICLE::SET_VEHICLE_ENGINE_ON(mReplayVehicle, true, true, false);
                mLastNode = mActiveReplay->Nodes.begin();
            }
            else {
                break;
            }
            [[fallthrough]];
        }
        case EReplayState::Playing: {
            auto replayTime = gameTime - replayStart;
            auto nodeCurr = std::upper_bound(mLastNode, mActiveReplay->Nodes.end(), SReplayNode{ replayTime });
            if (nodeCurr != mActiveReplay->Nodes.begin())
                nodeCurr = std::prev(nodeCurr);

            mLastNode = nodeCurr;

            bool lastNode = nodeCurr == std::prev(mActiveReplay->Nodes.end());
            if (nodeCurr == mActiveReplay->Nodes.end() || lastNode) {
                mReplayState = EReplayState::Finished;
                //UI::Notify("Replay finished", false);
                break;
            }

            auto nodeNext = lastNode ? nodeCurr : std::next(nodeCurr);

            float progress = ((float)replayTime - (float)nodeCurr->Timestamp) / 
                ((float)nodeNext->Timestamp - (float)nodeCurr->Timestamp);

            Vector3 pos = lerp(nodeCurr->Pos, nodeNext->Pos, progress);
            Vector3 rot = lerp(nodeCurr->Rot, nodeNext->Rot, progress, -180.0f, 180.0f);
            ENTITY::SET_ENTITY_COORDS_NO_OFFSET(mReplayVehicle, pos.x, pos.y, pos.z, false, false, false);
            ENTITY::SET_ENTITY_ROTATION(mReplayVehicle, rot.x, rot.y, rot.z, 0, false);

            if (VExt::GetNumWheels(mReplayVehicle) == nodeCurr->WheelRotations.size()) {
                for (uint8_t idx = 0; idx < VExt::GetNumWheels(mReplayVehicle); ++idx) {
                    float wheelRot = lerp(nodeCurr->WheelRotations[idx], nodeNext->WheelRotations[idx], progress,
                        -static_cast<float>(M_PI), static_cast<float>(M_PI));
                    VExt::SetWheelRotation(mReplayVehicle, idx, wheelRot);
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

            if (VExt::GetNumWheels(mReplayVehicle) == nodeCurr->SuspensionCompressions.size()) {
                for (uint8_t idx = 0; idx < VExt::GetNumWheels(mReplayVehicle); ++idx) {
                    float susp = lerp(nodeCurr->SuspensionCompressions[idx], nodeNext->SuspensionCompressions[idx], progress);
                    VExt::SetWheelCompression(mReplayVehicle, idx, susp);
                }
            }

            VEHICLE::SET_VEHICLE_BRAKE_LIGHTS(mReplayVehicle, nodeCurr->Brake > 0.1f);

            VEHICLE::SET_VEHICLE_LIGHTS(mReplayVehicle, nodeCurr->LowBeams ? 3 : 4);
            VEHICLE::SET_VEHICLE_FULLBEAM(mReplayVehicle, nodeCurr->HighBeams);

            if (replayTime >= 5000 && finishPassedThisTick) {
                if (startPassedThisTick) {
                    mReplayState = EReplayState::Playing;
                    replayStart = gameTime;
                    ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, true, true);
                    ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, map(mSettings.Replay.VehicleAlpha, 0, 100, 0, 255), false);
                    ENTITY::SET_ENTITY_COMPLETELY_DISABLE_COLLISION(mReplayVehicle, false, false);
                    VEHICLE::SET_VEHICLE_ENGINE_ON(mReplayVehicle, true, true, false);
                    mLastNode = mActiveReplay->Nodes.begin();
                }
                else {
                    mReplayState = EReplayState::Finished;
                }
                //UI::Notify("Player finished", false);
            }
            break;
        }
        case EReplayState::Finished: {
            mReplayState = EReplayState::Idle;
            ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, 0, true);
            ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, false, false);
            VExt::SetCurrentRPM(mReplayVehicle, 0.0f);
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
                mCurrentRun.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>
                    (std::chrono::system_clock::now().time_since_epoch()).count();
                mCurrentRun.Track = mActiveTrack->Name;
                mCurrentRun.Nodes.clear();
                mCurrentRun.VehicleModel = ENTITY::GET_ENTITY_MODEL(vehicle);

                VehicleModData modData = VehicleModData::LoadFrom(vehicle);
                mCurrentRun.VehicleMods = modData;
                //UI::Notify("Record started", false);
            }
            else {
                break;
            }
            [[fallthrough]];
        }
        case ERecordState::Recording: {
            if (ENTITY::GET_ENTITY_MODEL(vehicle) != mCurrentRun.VehicleModel) {
                mRecordState = ERecordState::Finished;
                break;
            }
            SReplayNode node{};
            node.Timestamp = gameTime - recordStart;
            node.Pos = nowPos;
            node.Rot = nowRot;
            node.WheelRotations = VExt::GetWheelRotations(vehicle);
            node.SuspensionCompressions = VExt::GetWheelCompressions(vehicle);

            node.SteeringAngle = VExt::GetSteeringAngle(vehicle);
            node.Throttle = VExt::GetThrottleP(vehicle);
            node.Brake = VExt::GetBrakeP(vehicle);

            node.Gear = VExt::GetGearCurr(vehicle);
            node.RPM = VExt::GetCurrentRPM(vehicle);

            BOOL areLowBeamsOn, areHighBeamsOn;
            VEHICLE::GET_VEHICLE_LIGHTS_STATE(vehicle, &areLowBeamsOn, &areHighBeamsOn);
            node.LowBeams = areLowBeamsOn;
            node.HighBeams = areHighBeamsOn;

            // Only make use of all that stuff if ExtensiveReplayTelemetry is used
            if (mSettings.Main.ExtensiveReplayTelemetry) {
                node.VehicleSpeed = ENTITY::GET_ENTITY_SPEED(vehicle);
            }
            else {
                node.VehicleSpeed = 0.0f;
            }

            bool saved = false;
            unsigned long long lastNodeTime = 0;
            if (!mCurrentRun.Nodes.empty()) {
                lastNodeTime = mCurrentRun.Nodes.back().Timestamp;
            }
            if (node.Timestamp >= lastNodeTime + mSettings.Record.DeltaMillis) {
                mCurrentRun.Nodes.push_back(node);
                saved = true;
            }

            if (node.Timestamp >= 5000 && finishPassedThisTick) {
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
                    SetReplay(mCurrentRun.Name, mCurrentRun.Timestamp);
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

                if (startPassedThisTick) {
                    mCurrentRun = CReplayData("");
                    mRecordState = ERecordState::Recording;
                    recordStart = gameTime;
                    mCurrentRun.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>
                        (std::chrono::system_clock::now().time_since_epoch()).count();
                    mCurrentRun.Track = mActiveTrack->Name;
                    mCurrentRun.Nodes.clear();
                    mCurrentRun.VehicleModel = ENTITY::GET_ENTITY_MODEL(vehicle);

                    VehicleModData modData = VehicleModData::LoadFrom(vehicle);
                    mCurrentRun.VehicleMods = modData;
                    //UI::Notify("Record started", false);
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

void CReplayScript::clearPtfx() {
    for(int i : mPtfxHandles) {
        GRAPHICS::STOP_PARTICLE_FX_LOOPED(i, 0);
    }
    mPtfxHandles.clear();
}

void CReplayScript::createPtfx(const CTrackData& trackData) {
    const char* assetName = "scr_apartment_mp";
    const char* effectName = "scr_finders_package_flare";
    auto startTime = GetTickCount64();
    STREAMING::REQUEST_NAMED_PTFX_ASSET(assetName);
    while (!STREAMING::HAS_NAMED_PTFX_ASSET_LOADED(assetName)) {
        WAIT(0);
        if (GetTickCount64() > startTime + 5000) {
            WAIT(0);
            std::string msg = fmt::format("Error: Failed to load flare assets.");
            UI::Notify(msg, false);
            logger.Write(ERROR, fmt::format("[Replay] {}", msg));
            return;
        }
    }

    Vector3 emptyVec{};
    if (trackData.StartLine.A != emptyVec &&
        trackData.StartLine.B != emptyVec) {
        Vector3 A = Util::GroundZ(trackData.StartLine.A, 0.1f);
        Vector3 B = Util::GroundZ(trackData.StartLine.B, 0.1f);

        mPtfxHandles.push_back(Util::CreateParticleFxAtCoord(assetName, effectName, A, 0.0f, 1.0f, 0.0f, 1.0f));
        mPtfxHandles.push_back(Util::CreateParticleFxAtCoord(assetName, effectName, B, 0.0f, 1.0f, 0.0f, 1.0f));
    }

    if (trackData.FinishLine.A != emptyVec &&
        trackData.FinishLine.B != emptyVec) {
        Vector3 A = Util::GroundZ(trackData.FinishLine.A, 0.1f);
        Vector3 B = Util::GroundZ(trackData.FinishLine.B, 0.1f);

        mPtfxHandles.push_back(Util::CreateParticleFxAtCoord(assetName, effectName, A, 1.0f, 0.0f, 0.0f, 1.0f));
        mPtfxHandles.push_back(Util::CreateParticleFxAtCoord(assetName, effectName, B, 1.0f, 0.0f, 0.0f, 1.0f));
    }
}
