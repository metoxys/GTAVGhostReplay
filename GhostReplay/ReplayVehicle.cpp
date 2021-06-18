#include "ReplayVehicle.hpp"
#include "Script.hpp"
#include "Util/UI.hpp"
#include "Util/Logger.hpp"
#include "Util/Math.hpp"
#include "Memory/VehicleExtensions.hpp"
#include <inc/natives.h>
#include <fmt/format.h>

using VExt = VehicleExtensions;

CReplayVehicle::CReplayVehicle(const CScriptSettings& settings, CReplayData* activeReplay,
    const std::function<void()>& onCleanup)
    : mSettings(settings)
    , mActiveReplay(activeReplay)
    , mReplayVehicle(0)
    , mReplayState(EReplayState::Idle)
    , mOnCleanup(onCleanup) {
    mLastNode = activeReplay->Nodes.begin();
    createReplayVehicle(activeReplay->VehicleModel, activeReplay, activeReplay->Nodes[0].Pos);
}

CReplayVehicle::~CReplayVehicle() {
    if (!Dll::Unloading())
        resetReplay();
}

void CReplayVehicle::UpdatePlayback(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick) {
    if (!mActiveReplay) {
        mReplayState = EReplayState::Idle;
        resetReplay();
        return;
    }

    switch (mReplayState) {
        case EReplayState::Idle: {
            if (startPassedThisTick) {
                mReplayState = EReplayState::Playing;
                startReplay(gameTime);
            }
            break;
        }
        case EReplayState::Playing: {
            auto replayTime = gameTime - replayStart;
            replayTime += (unsigned long long)(mSettings.Replay.OffsetSeconds * 1000.0f);
            auto nodeCurr = std::upper_bound(mLastNode, mActiveReplay->Nodes.end(), SReplayNode{ replayTime });
            if (nodeCurr != mActiveReplay->Nodes.begin())
                nodeCurr = std::prev(nodeCurr);

            mLastNode = nodeCurr;

            bool lastNode = nodeCurr == std::prev(mActiveReplay->Nodes.end());
            if (nodeCurr == mActiveReplay->Nodes.end() || lastNode) {
                mReplayState = EReplayState::Idle;
                resetReplay();
                //UI::Notify("Replay finished", false);
                break;
            }
            showNode(replayTime, lastNode, nodeCurr);

            // On a faster player lap, the current replay is nuked and this doesn't run, but better to
            // not rely on that behavior and have this here anyway, in case that part changes.
            if (finishPassedThisTick) {
                mReplayState = EReplayState::Idle;
                resetReplay();
                if (startPassedThisTick) {
                    mReplayState = EReplayState::Playing;
                    startReplay(gameTime);
                }
            }
            break;
        }
    }
}

void CReplayVehicle::startReplay(unsigned long long gameTime) {
    replayStart = gameTime;
    ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, true, true);
    ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, map(mSettings.Replay.VehicleAlpha, 0, 100, 0, 255), false);
    ENTITY::SET_ENTITY_COMPLETELY_DISABLE_COLLISION(mReplayVehicle, false, false);
    VEHICLE::SET_VEHICLE_ENGINE_ON(mReplayVehicle, true, true, false);
    mLastNode = mActiveReplay->Nodes.begin();

    if (mSettings.Main.GhostBlips)
        createBlip();
}

void CReplayVehicle::showNode(unsigned long long replayTime, bool lastNode, const std::vector<SReplayNode>::iterator& nodeCurr) {
    auto nodeNext = lastNode ? nodeCurr : std::next(nodeCurr);
    float progress = ((float)replayTime - (float)nodeCurr->Timestamp) /
        ((float)nodeNext->Timestamp - (float)nodeCurr->Timestamp);
    float deltaT = ((nodeNext->Timestamp - nodeCurr->Timestamp) * 0.001f); // Seconds
    Vector3 pos = lerp(nodeCurr->Pos, nodeNext->Pos, progress);
    Vector3 rot = lerp(nodeCurr->Rot, nodeNext->Rot, progress, -180.0f, 180.0f);
    ENTITY::SET_ENTITY_COORDS_NO_OFFSET(mReplayVehicle, pos.x, pos.y, pos.z, false, false, false);
    ENTITY::SET_ENTITY_ROTATION(mReplayVehicle, rot.x, rot.y, rot.z, 0, false);

    Vector3 vel = (nodeNext->Pos - nodeCurr->Pos) * (1.0f / deltaT);
    ENTITY::SET_ENTITY_VELOCITY(mReplayVehicle, vel.x, vel.y, vel.z);

    if (VExt::GetNumWheels(mReplayVehicle) == nodeCurr->WheelRotations.size()) {
        for (uint8_t idx = 0; idx < VExt::GetNumWheels(mReplayVehicle); ++idx) {
            float wheelRot = lerp(nodeCurr->WheelRotations[idx], nodeNext->WheelRotations[idx], progress,
                -static_cast<float>(M_PI), static_cast<float>(M_PI));
            VExt::SetWheelRotation(mReplayVehicle, idx, wheelRot);

            float wheelRotVel = (nodeNext->WheelRotations[idx] - nodeCurr->WheelRotations[idx]) / deltaT;
            VExt::SetWheelRotationSpeed(mReplayVehicle, idx, wheelRotVel);
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

    switch (mSettings.Replay.ForceLights) {
        case 2: // Force On
            VEHICLE::SET_VEHICLE_LIGHTS(mReplayVehicle, 3);
            VEHICLE::SET_VEHICLE_FULLBEAM(mReplayVehicle, false);
            break;
        case 1: // Force Off
            VEHICLE::SET_VEHICLE_LIGHTS(mReplayVehicle, 4);
            VEHICLE::SET_VEHICLE_FULLBEAM(mReplayVehicle, false);
            break;
        case 0: // Default
        default:
            VEHICLE::SET_VEHICLE_LIGHTS(mReplayVehicle, nodeCurr->LowBeams ? 3 : 4);
            VEHICLE::SET_VEHICLE_FULLBEAM(mReplayVehicle, nodeCurr->HighBeams);
    }
}

void CReplayVehicle::resetReplay() {
    ENTITY::SET_ENTITY_VISIBLE(mReplayVehicle, false, false);
    ENTITY::SET_ENTITY_ALPHA(mReplayVehicle, 0, false);
    ENTITY::SET_ENTITY_COLLISION(mReplayVehicle, false, false);
    VExt::SetCurrentRPM(mReplayVehicle, 0.0f);
    mLastNode = mActiveReplay->Nodes.begin();
    deleteBlip();
    
    if (mOnCleanup)
        mOnCleanup();
}

void CReplayVehicle::createReplayVehicle(Hash model, CReplayData* activeReplay, Vector3 pos) {
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

void CReplayVehicle::createBlip() {
    mReplayVehicleBlip = std::make_unique<CWrappedBlip>(
        mReplayVehicle,
        eBlipSprite::BlipSpriteStandard,
        mActiveReplay->Name,
        eBlipColor::BlipColorWhite, true);
}

void CReplayVehicle::deleteBlip() {
    mReplayVehicleBlip.reset();
}
