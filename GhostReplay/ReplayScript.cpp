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
    std::vector<std::shared_ptr<CReplayData>>& replays,
    std::vector<CTrackData>& tracks,
    std::vector<CImage>& trackImages,
    std::vector<CTrackData>& arsTracks)
    : mCurrentTime(static_cast<double>(MISC::GET_GAME_TIMER()))
    , mSettings(settings)
    , mReplays(replays)
    , mCompatibleReplays()
    , mTracks(tracks)
    , mTrackImages(trackImages)
    , mArsTracks(arsTracks)
    , mActiveReplay(nullptr)
    , mActiveTrack(nullptr)
    , mScriptMode(EScriptMode::ReplayActive)
    , mRecordState(ERecordState::Idle)
    , mLastPos(Vector3{})
    , mReplayVehicle(nullptr)
    , mCurrentRun("") {
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
    updateSteppedTime();

    // UI::ShowText(0.65f, 0.30f, 0.5f, fmt::format("{:.9f}", MISC::GET_FRAME_TIME()));
    // UI::ShowText(0.65f, 0.35f, 0.5f, fmt::format("{:.9f}", mCurrentTime));
    // UI::ShowText(0.65f, 0.40f, 0.5f, fmt::format("{:.9f}", getSteppedTime()));

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
    clearTrackBlips();

    if (trackName.empty()) {
        mActiveTrack = nullptr;
        mReplayVehicle.reset();
        mRecordState = ERecordState::Idle;
        if (RagePresence::AreCustomDetailsSet())
            RagePresence::ClearCustomDetails();
        if (RagePresence::IsCustomStateSet())
            RagePresence::ClearCustomState();
        return;
    }

    CTrackData* foundTrack = nullptr;
    if (!trackName._Starts_with("[ARS] ")) {
        for (auto& track : mTracks) {
            if (track.Name == trackName) {
                foundTrack = &track;
                break;
            }
        }
    }
    else {
        for (auto& track : mArsTracks) {
            if (track.Name == trackName) {
                foundTrack = &track;
                break;
            }
        }
    }

    if (foundTrack != nullptr) {
        bool alone = true;

        mActiveTrack = foundTrack;
        mReplayVehicle.reset();
        mRecordState = ERecordState::Idle;
        mCompatibleReplays = GetCompatibleReplays(trackName);
        if (mSettings.Main.DrawStartFinish)
            createPtfx(*foundTrack);

        if (mSettings.Main.StartStopBlips)
            createTrackBlips(*foundTrack);

        if (mSettings.Replay.AutoLoadGhost) {
            Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);
            auto fastestReplay = GetFastestReplay(trackName, ENTITY::GET_ENTITY_MODEL(vehicle));
            if (!fastestReplay.Name.empty() && ENTITY::DOES_ENTITY_EXIST(vehicle)) {
                SetReplay(fastestReplay.Name, fastestReplay.Timestamp);
                alone = false;
            }
        }

        if (alone)
            RagePresence::SetCustomDetails("Setting a new laptime");
        RagePresence::SetCustomState(fmt::format("GhostReplay Track: {}", foundTrack->Name).c_str());

        return;
    }

    logger.Write(ERROR, "SetTrack: No track found with the name [%s]", trackName.c_str());
}

void CReplayScript::SetReplay(const std::string& replayName, unsigned long long timestamp) {
    DeactivatePassengerMode();
    if (replayName.empty()) {
        mActiveReplay = nullptr;
        mRecordState = ERecordState::Idle;
        mReplayVehicle.reset();
        return;
    }

    for (auto& replay : mReplays) {
        bool nameOK = replay->Name == replayName;
        bool timeOK = timestamp == 0 ? true : replay->Timestamp == timestamp;

        if (nameOK && timeOK) {
            mActiveReplay = &*replay;
            mRecordState = ERecordState::Idle;
            mReplayVehicle = std::make_unique<CReplayVehicle>(mSettings, mActiveReplay,
                std::bind(static_cast<void(CReplayScript::*)(int)>(&CReplayScript::DeactivatePassengerMode), this, std::placeholders::_1));

            std::string opponent = Util::GetVehicleName(replay->VehicleModel);
            std::string timestamp = Util::FormatMillisTime(replay->Nodes.back().Timestamp);
            RagePresence::SetCustomDetails(fmt::format("Racing ghost {} | {}", opponent, timestamp).c_str());
            return;
        }
    }

    // createReplayVehicle should've notified already if we're here.
    if (mReplayVehicle->GetVehicle() == 0) {
        mActiveReplay = nullptr;
        mReplayVehicle.reset();
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

std::vector<std::shared_ptr<CReplayData>> CReplayScript::GetCompatibleReplays(const std::string& trackName) {
    std::vector<std::shared_ptr<CReplayData>> replays;
    for (auto& replay : mReplays) {
        if (replay->Track == trackName)
            replays.push_back(replay);
    }
    return replays;
}

void CReplayScript::AddCompatibleReplay(const CReplayData& value) {
    mCompatibleReplays.push_back(std::make_shared<CReplayData>(value));
}

bool CReplayScript::IsFastestLap(const std::string& trackName, Hash vehicleModel, double timestamp) {
    CReplayData fastestReplay = GetFastestReplay(trackName, vehicleModel);

    if (!fastestReplay.Name.empty())
        return timestamp < fastestReplay.Nodes.back().Timestamp;

    return false;
}

CReplayData CReplayScript::GetFastestReplay(const std::string& trackName, Hash vehicleModel) {
    CReplayData fastestReplay("");

    for (auto& replay : mReplays) {
        if (replay->Track != trackName)
            continue;

        if (vehicleModel != 0 && replay->VehicleModel != vehicleModel)
            continue;

        if (fastestReplay.Nodes.empty() ||
            replay->Nodes.back().Timestamp < fastestReplay.Nodes.back().Timestamp) {
            fastestReplay = *replay;
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
    auto replayIt = std::find_if(mReplays.begin(), mReplays.end(), [&](const auto& x) {
        return x->FileName() == replay.FileName();
    });
    // Because it's a copy, let's just update this in parallel.
    auto replayCompIt = std::find_if(mCompatibleReplays.begin(), mCompatibleReplays.end(), [&](const auto& x) {
        return x->FileName() == replay.FileName();
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

void CReplayScript::ActivatePassengerMode() {
    if (!mActiveTrack || !mActiveReplay) {
        mPassengerModeActive = false;
        mPassengerModePlayerVehicle = 0;
        return;
    }

    StopRecording();

    Ped playerPed = PLAYER::PLAYER_PED_ID();
    Vehicle playerVehicle = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);
    if (ENTITY::DOES_ENTITY_EXIST(playerVehicle)) {
        mPassengerModePlayerVehicle = playerVehicle;
        if (ENTITY::IS_ENTITY_A_MISSION_ENTITY(playerVehicle)) {
            mPassengerModPlayerVehicleManagedByThisScript = false;
        }
        else {
            ENTITY::SET_ENTITY_AS_MISSION_ENTITY(playerVehicle, true, true);
            mPassengerModPlayerVehicleManagedByThisScript = true;
        }

        auto pos = ENTITY::GET_ENTITY_COORDS(playerVehicle, !ENTITY::IS_ENTITY_DEAD(playerVehicle, false));
        ENTITY::FREEZE_ENTITY_POSITION(playerVehicle, true);
        ENTITY::SET_ENTITY_COORDS_NO_OFFSET(playerVehicle, pos.x, pos.y, pos.z + 100.0f, 0, 0, 0);
        ENTITY::SET_ENTITY_VISIBLE(playerVehicle, false, false);
    }

    mReplayVehicle->UpdatePlayback(getSteppedTime(), true, false);

    Vehicle replayVehicle = mReplayVehicle->GetVehicle();
    int numReplayVehicleSeats = VEHICLE::GET_VEHICLE_MODEL_NUMBER_OF_SEATS(ENTITY::GET_ENTITY_MODEL(replayVehicle));

    if (numReplayVehicleSeats > 1) {
        PED::SET_PED_INTO_VEHICLE(playerPed, replayVehicle, -2);
    }
    else {
        PED::SET_PED_INTO_VEHICLE(playerPed, replayVehicle, -1);
    }
    mPassengerModeActive = true;
}

void CReplayScript::DeactivatePassengerMode() {
    if (mReplayVehicle) {
        DeactivatePassengerMode(mReplayVehicle->GetVehicle());
    }
}

void CReplayScript::DeactivatePassengerMode(Vehicle vehicle) {
    if (!mPassengerModeActive) {
        return;
    }

    Ped playerPed = PLAYER::PLAYER_PED_ID();
    Vehicle playerVehicle = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);
    if (playerVehicle != vehicle) {
        mPassengerModeActive = false;
        return;
    }
    Vehicle replayVehicle = vehicle;

    Vector3 currRot = ENTITY::GET_ENTITY_ROTATION(replayVehicle, 0);
    Vector3 currCoords = ENTITY::GET_ENTITY_COORDS(replayVehicle, !ENTITY::IS_ENTITY_DEAD(replayVehicle, false));
    Vector3 forward = ENTITY::GET_ENTITY_FORWARD_VECTOR(replayVehicle);
    currCoords.x -= forward.x * 10.0f;
    currCoords.y -= forward.y * 10.0f;
    float groundZ = currCoords.z;
    MISC::GET_GROUND_Z_FOR_3D_COORD(currCoords.x, currCoords.y, currCoords.z, &groundZ, 0, 0);

    if (mPassengerModePlayerVehicle) {
        PED::SET_PED_INTO_VEHICLE(playerPed, mPassengerModePlayerVehicle, -1);

        ENTITY::SET_ENTITY_COORDS(mPassengerModePlayerVehicle, currCoords.x, currCoords.y, groundZ,
            false, false, false, false);
        ENTITY::SET_ENTITY_ROTATION(mPassengerModePlayerVehicle, currRot.x, currRot.y, currRot.z, 0, false);

        ENTITY::FREEZE_ENTITY_POSITION(mPassengerModePlayerVehicle, false);
        ENTITY::SET_ENTITY_VISIBLE(mPassengerModePlayerVehicle, true, false);

        if (mPassengerModPlayerVehicleManagedByThisScript) {
            ENTITY::SET_ENTITY_AS_MISSION_ENTITY(mPassengerModePlayerVehicle, false, true);
            ENTITY::SET_ENTITY_AS_NO_LONGER_NEEDED(&mPassengerModePlayerVehicle);

            VEHICLE::SET_VEHICLE_FIXED(mPassengerModePlayerVehicle);
            VEHICLE::SET_VEHICLE_DEFORMATION_FIXED(mPassengerModePlayerVehicle);

            mPassengerModPlayerVehicleManagedByThisScript = false;
        }

        mPassengerModePlayerVehicle = 0;
    }
    else {
        ENTITY::SET_ENTITY_COORDS(playerPed, currCoords.x, currCoords.y, groundZ,
            false, false, false, false);
    }

    mPassengerModeActive = false;
}

double CReplayScript::GetReplayProgress() {
    if (mReplayVehicle)
        return mReplayVehicle->GetReplayProgress();
    return 0;
}

void CReplayScript::TogglePause(bool pause) {
    if (mReplayVehicle)
        mReplayVehicle->TogglePause(pause, getSteppedTime());
}

void CReplayScript::ScrubBackward(double step) {
    if (mReplayVehicle)
        mReplayVehicle->ScrubBackward(step);
}

void CReplayScript::ScrubForward(double step) {
    if (mReplayVehicle)
        mReplayVehicle->ScrubForward(step);
}

uint64_t CReplayScript::GetNumFrames() {
    if (mReplayVehicle)
        return mReplayVehicle->GetNumFrames();
    return 0;
}

uint64_t CReplayScript::GetFrameIndex() {
    if (mReplayVehicle)
        return mReplayVehicle->GetFrameIndex();
    return 0;
}

void CReplayScript::FramePrev() {
    if (mReplayVehicle)
        mReplayVehicle->FramePrev();
}

void CReplayScript::FrameNext() {
    if (mReplayVehicle)
        mReplayVehicle->FrameNext();
}

void CReplayScript::TeleportToTrack(const CTrackData& trackData) {
    Ped playerPed = PLAYER::PLAYER_PED_ID();
    Vehicle playerVehicle = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);

    if (mReplayVehicle && playerVehicle) {
        if (mReplayVehicle->GetVehicle() == playerVehicle) {
            UI::Notify("Can't teleport while riding ghost vehicle.");
            return;
        }
    }

    Vector3 startLineAB = (trackData.StartLine.A + trackData.StartLine.B) * 0.5f;
    Vector3 startOffset = GetPerpendicular(startLineAB, trackData.StartLine.B, 15.0f, false);
    float startHeading = MISC::GET_HEADING_FROM_VECTOR_2D(
        trackData.StartLine.B.x - trackData.StartLine.A.x,
        trackData.StartLine.B.y - trackData.StartLine.A.y
    ) + 90.0f;

    if (playerVehicle) {
        ENTITY::SET_ENTITY_COORDS(playerVehicle, startOffset.x, startOffset.y, startOffset.z,
            false, false, false, false);
        ENTITY::SET_ENTITY_HEADING(playerVehicle, startHeading);
    }
    else {
        ENTITY::SET_ENTITY_COORDS(playerPed, startOffset.x, startOffset.y, startOffset.z,
            false, false, false, false);
        ENTITY::SET_ENTITY_HEADING(playerPed, startHeading);
    }
}

void CReplayScript::updateReplay() {
    if (!mActiveTrack)
        return;

    Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);
    Vector3 nowPos;

    bool inGhostVehicle = false;
    if (mReplayVehicle)
        inGhostVehicle = vehicle == mReplayVehicle->GetVehicle();

    if (ENTITY::DOES_ENTITY_EXIST(vehicle)) {
        nowPos = ENTITY::GET_ENTITY_COORDS(vehicle, true);
    }
    else {
        nowPos = mLastPos;
    }

    auto gameTime = getSteppedTime();
    bool startPassedThisTick = passedLineThisTick(mActiveTrack->StartLine, mLastPos, nowPos);
    bool finishPassedThisTick = passedLineThisTick(mActiveTrack->FinishLine, mLastPos, nowPos);

    // Prevents default coords from activating record/replay
    if (mLastPos == Vector3{}) {
        startPassedThisTick = false;
        finishPassedThisTick = false;
    }

    mLastPos = nowPos;

    updateRecord(gameTime, !inGhostVehicle && startPassedThisTick, !inGhostVehicle && finishPassedThisTick);
    if (mReplayVehicle)
        mReplayVehicle->UpdatePlayback(gameTime, startPassedThisTick, finishPassedThisTick);
}

void CReplayScript::updateRecord(double gameTime, bool startPassedThisTick, bool finishPassedThisTick) {
    Vehicle vehicle = PED::GET_VEHICLE_PED_IS_IN(PLAYER::PLAYER_PED_ID(), false);
    if (!ENTITY::DOES_ENTITY_EXIST(vehicle) || !mActiveTrack) {
        mRecordState = ERecordState::Idle;
        mCurrentRun = CReplayData("");
        return;
    }

    switch (mRecordState) {
        case ERecordState::Idle: {
            if (startPassedThisTick) {
                mRecordState = ERecordState::Recording;
                startRecord(gameTime, vehicle);
                SReplayNode node{};
                saveNode(gameTime, node, vehicle, true);
            }
            break;
        }
        case ERecordState::Recording: {
            if (ENTITY::GET_ENTITY_MODEL(vehicle) != mCurrentRun.VehicleModel) {
                mRecordState = ERecordState::Idle;
                mCurrentRun = CReplayData("");
                break;
            }
            SReplayNode node{};
            bool saved = saveNode(gameTime, node, vehicle, false);

            if (finishPassedThisTick) {
                mRecordState = ERecordState::Idle;
                finishRecord(saved, node);
                if (startPassedThisTick) {
                    mRecordState = ERecordState::Recording;
                    startRecord(gameTime, vehicle);
                }
            }
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

void CReplayScript::startRecord(double gameTime, Vehicle vehicle) {
    mRecordStart = gameTime;
    mCurrentRun.Timestamp = std::chrono::duration_cast<std::chrono::milliseconds>
        (std::chrono::system_clock::now().time_since_epoch()).count();
    mCurrentRun.Track = mActiveTrack->Name;
    mCurrentRun.Nodes.clear();
    mCurrentRun.VehicleModel = ENTITY::GET_ENTITY_MODEL(vehicle);

    VehicleModData modData = VehicleModData::LoadFrom(vehicle);
    mCurrentRun.VehicleMods = modData;
    //UI::Notify("Record started", false);
}

bool CReplayScript::saveNode(double gameTime, SReplayNode& node, Vehicle vehicle, bool firstNode) {
    Vector3 nowPos = ENTITY::GET_ENTITY_COORDS(vehicle, true);
    Vector3 nowRot = ENTITY::GET_ENTITY_ROTATION(vehicle, 0);

    node.Timestamp = gameTime - mRecordStart;
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

    bool saved = false;
    double lastNodeTime = 0.0;
    if (!mCurrentRun.Nodes.empty()) {
        lastNodeTime = mCurrentRun.Nodes.back().Timestamp;
    }
    if (node.Timestamp >= lastNodeTime + mSettings.Record.DeltaMillis || firstNode) {
        mCurrentRun.Nodes.push_back(node);
        saved = true;
    }
    return saved;
}

void CReplayScript::finishRecord(bool saved, const SReplayNode& node) {
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

    if (mSettings.Record.AutoGhost && (!mActiveReplay || fasterLap)) {
        mCurrentRun.Name = Util::FormatReplayName(
            node.Timestamp,
            mCurrentRun.Track,
            Util::GetVehicleName(mCurrentRun.VehicleModel));
        CReplayData::WriteAsync(mCurrentRun);
        GhostReplay::AddReplay(mCurrentRun);
        mCompatibleReplays.push_back(std::make_shared<CReplayData>(mCurrentRun));
        SetReplay(mCurrentRun.Name, mCurrentRun.Timestamp);
    }
    else {
        mUnsavedRuns.push_back(mCurrentRun);
    }

    std::string lapInfo = fmt::format("Lap time: {}{}",
                                      fastestLap ? "~p~" : fasterLap ? "~g~" : "~y~",
                                      Util::FormatMillisTime(node.Timestamp));

    // TODO: Movable UI element with current time & laptime & delta?

    if (mSettings.Main.NotifyLaps) {
        UI::Notify(lapInfo, false);
    }
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

void CReplayScript::clearTrackBlips() {
    mStartBlip.reset();
    mFinishBlip.reset();
}

void CReplayScript::createTrackBlips(const CTrackData& trackData) {
    Vector3 startBlipCoord = (trackData.StartLine.A + trackData.StartLine.B) * 0.5f;
    Vector3 finishBlipCoord = (trackData.FinishLine.A + trackData.FinishLine.B) * 0.5f;

    // Skip finish blip if it's within 5 meters of the start blip
    float blipDist = Distance(startBlipCoord, finishBlipCoord);
    bool showFinish = blipDist > 5.0f;

    std::string startName = showFinish ? fmt::format("{} (Start)", trackData.Name) : trackData.Name;
    eBlipColor startColor = showFinish ? eBlipColor::BlipColorBlue : eBlipColor::BlipColorWhite;

    mStartBlip = std::make_unique<CWrappedBlip>(
        startBlipCoord,
        eBlipSprite::BlipSpriteRaceFinish,
        startName,
        startColor);

    if (showFinish) {
        mFinishBlip = std::make_unique<CWrappedBlip>(
            finishBlipCoord,
            eBlipSprite::BlipSpriteRaceFinish,
            fmt::format("{} (Finish)", trackData.Name),
            eBlipColor::BlipColorWhite);
    }
}

void CReplayScript::updateSteppedTime() {
    mCurrentTime = mCurrentTime + static_cast<double>(MISC::GET_FRAME_TIME());
}

double CReplayScript::getSteppedTime() {
    return mCurrentTime * 1000.0;
}
