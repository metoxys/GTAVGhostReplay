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
    CReplayData* GetReplay() { return mActiveReplay; }

    void UpdateCollision(bool enable);

    // Returns true when the replay was (re)started this tick.
    bool UpdatePlayback(double replayTime, bool startPassedThisTick, bool finishPassedThisTick);

    EReplayState GetReplayState() {
        return mReplayState;
    }

    void StopReplay() {
        mReplayState = EReplayState::Idle;
        resetReplay();
    }

    // Playback control
    double GetReplayProgress();
    void TogglePause(bool pause);
    void SetReplayTime(double replayTime);

    // Debugging playback
    uint64_t GetNumFrames();
    uint64_t GetFrameIndex();
    double FramePrev();
    double FrameNext();

private:
    const CScriptSettings& mSettings;
    CReplayData* mActiveReplay;

    Vehicle mReplayVehicle;
    std::unique_ptr<CWrappedBlip> mReplayVehicleBlip;

    EReplayState mReplayState;
    std::vector<SReplayNode>::iterator mLastNode;

    std::function<void(Vehicle)> mOnCleanup;

    void startReplay();
    void showNode(double replayTime,
        std::vector<SReplayNode>::iterator nodeCurr,
        std::vector<SReplayNode>::iterator nodeNext);
    void resetReplay();

    void createReplayVehicle(Hash model, CReplayData* activeReplay, Vector3 pos);
    void createBlip();
    void deleteBlip();

    void hideVehicle();
    void unhideVehicle();
};
