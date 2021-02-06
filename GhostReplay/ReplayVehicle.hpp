#pragma once
#include "Blip.hpp"
#include "ReplayData.hpp"
#include "ScriptSettings.hpp"
#include <inc/types.h>

enum class EReplayState {
    Idle,
    Playing,
    Finished,
};

class CReplayVehicle {
public:
    CReplayVehicle(const CScriptSettings& settings, CReplayData* activeReplay);

    CReplayVehicle(const CReplayVehicle&) = delete;
    CReplayVehicle& operator=(const CReplayVehicle&) = delete;
    CReplayVehicle(CReplayVehicle&&) = delete;
    CReplayVehicle& operator=(CReplayVehicle&&) = delete;

    ~CReplayVehicle() = default;

    Vehicle GetVehicle() { return mReplayVehicle; }

    void UpdatePlayback(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick);

    EReplayState GetReplayState() {
        return mReplayState;
    }

    void SetReplayState(EReplayState replayState) {
        mReplayState = replayState;
    }

private:
    const CScriptSettings& mSettings;
    CReplayData* mActiveReplay;

    Vehicle mReplayVehicle;
    std::unique_ptr<CWrappedBlip> mReplayVehicleBlip;

    EReplayState mReplayState;
    unsigned long long replayStart = 0;
    std::vector<SReplayNode>::iterator mLastNode;

    void createReplayVehicle(Hash model, CReplayData* activeReplay, Vector3 pos);
    void createBlip();
    void deleteBlip();
};
