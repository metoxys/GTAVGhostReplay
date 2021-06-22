#pragma once
#include "Blip.hpp"
#include "ReplayData.hpp"
#include "ScriptSettings.hpp"
#include <inc/types.h>

enum class EReplayState {
    Idle,
    Playing,
    Paused,
};

class CReplayVehicle {
public:
    CReplayVehicle(const CScriptSettings& settings, CReplayData* activeReplay, const std::function<void(Vehicle)>& onCleanup);

    CReplayVehicle(const CReplayVehicle&) = delete;
    CReplayVehicle& operator=(const CReplayVehicle&) = delete;
    CReplayVehicle(CReplayVehicle&&) = delete;
    CReplayVehicle& operator=(CReplayVehicle&&) = delete;

    ~CReplayVehicle();

    Vehicle GetVehicle() { return mReplayVehicle; }

    void UpdatePlayback(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick);

    EReplayState GetReplayState() {
        return mReplayState;
    }

    void StopReplay() {
        mReplayState = EReplayState::Idle;
        resetReplay();
    }

    // Playback control
    uint64_t GetReplayProgress();
    void TogglePause(bool pause, uint64_t gameTime);
    void ScrubBackward(uint64_t millis);
    void ScrubForward(uint64_t millis);

    // Debugging playback
    uint64_t GetNumFrames();
    uint64_t GetFrameIndex();
    void FramePrev();
    void FrameNext();

private:
    const CScriptSettings& mSettings;
    CReplayData* mActiveReplay;

    Vehicle mReplayVehicle;
    std::unique_ptr<CWrappedBlip> mReplayVehicleBlip;

    EReplayState mReplayState;
    unsigned long long mReplayStart = 0;
    std::vector<SReplayNode>::iterator mLastNode;

    std::function<void(Vehicle)> mOnCleanup;

    void startReplay(unsigned long long gameTime);
    void showNode(unsigned long long replayTime,
        std::vector<SReplayNode>::iterator nodeCurr,
        std::vector<SReplayNode>::iterator nodeNext);
    void resetReplay();

    void createReplayVehicle(Hash model, CReplayData* activeReplay, Vector3 pos);
    void createBlip();
    void deleteBlip();

    void hideVehicle();
    void unhideVehicle();
};
