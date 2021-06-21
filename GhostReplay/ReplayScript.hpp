#pragma once
#include "ScriptSettings.hpp"
#include "ReplayData.hpp"
#include "TrackData.hpp"
#include "Image.hpp"
#include "Blip.hpp"
#include "ReplayVehicle.hpp"

#include <vector>
#include <string>

enum class EScriptMode {
    DefineTrack,
    ReplayActive,
};

enum class ERecordState {
    Idle,
    Recording,
};

class CReplayScript {
public:
    CReplayScript(
        CScriptSettings& settings,
        std::vector<std::shared_ptr<CReplayData>>& replays,
        std::vector<CTrackData>& tracks,
        std::vector<CImage>& trackImages,
        std::vector<CTrackData>& arsTracks);
    virtual ~CReplayScript();
    virtual void Tick();

    std::vector<std::shared_ptr<CReplayData>>& GetReplays() {
        return mCompatibleReplays;
    }

    std::vector<CTrackData>& GetTracks() {
        return mTracks;
    }

    std::vector<CTrackData>& GetARSTracks() {
        return mArsTracks;
    }

    const std::vector<CReplayData>& GetUnsavedRuns() const{
        return mUnsavedRuns;
    }

    CReplayData* ActiveReplay() const {
        return mActiveReplay;
    }

    CTrackData* ActiveTrack() const {
        return mActiveTrack;
    }

    EScriptMode ScriptMode() const {
        return mScriptMode;
    }

    void SetScriptMode(EScriptMode mode) {
        mScriptMode = mode;
    }

    EReplayState GetReplayState() {
        if (mReplayVehicle)
            return mReplayVehicle->GetReplayState();
        return EReplayState::Idle;
    }

    void StopReplay() {
        if (mReplayVehicle)
            mReplayVehicle->StopReplay();
    }

    bool IsRecording() {
        return mRecordState == ERecordState::Recording;
    }

    void StopRecording() {
        mRecordState = ERecordState::Idle;
        mCurrentRun = CReplayData("");
    }

    Vehicle GetReplayVehicle() {
        if (mReplayVehicle)
            return mReplayVehicle->GetVehicle();
        return 0;
    }

    void SetTrack(const std::string& trackName);
    void SetReplay(const std::string& replayName, unsigned long long timestamp = 0);
    void ClearUnsavedRuns();
    std::vector<CReplayData>::const_iterator EraseUnsavedRun(std::vector<CReplayData>::const_iterator runIt);

    std::vector<std::shared_ptr<CReplayData>> GetCompatibleReplays(const std::string& trackName);
    void AddCompatibleReplay(const CReplayData& value);

    bool IsFastestLap(const std::string& trackName, Hash vehicleModel, unsigned long long timestamp);
    CReplayData GetFastestReplay(const std::string& trackName, Hash vehicleModel);

    bool StartLineDef(SLineDef& lineDef, const std::string& lineName);
    void DeleteTrack(const CTrackData& track);
    void DeleteReplay(const CReplayData& replay);

    std::string GetTrackImageMenuString(const std::string& trackName);

    void ActivatePassengerMode();
    void DeactivatePassengerMode();
    void DeactivatePassengerMode(Vehicle vehicle);
    bool IsPassengerModeActive() { return mPassengerModeActive; }

    // Playback control
    uint64_t GetReplayProgress();
    void TogglePause(bool pause);
    void ScrubBackward(uint64_t millis);
    void ScrubForward(uint64_t millis);

    // Debugging playback
    uint64_t GetNumFrames();
    uint64_t GetFrameIndex();
    void FramePrev();
    void FrameNext();

    void TeleportToTrack(const CTrackData& trackData);

protected:
    void updateReplay();
    void updateRecord(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick);
    void updateTrackDefine();
    bool passedLineThisTick(SLineDef line, Vector3 oldPos, Vector3 newPos);
    void startRecord(unsigned long long gameTime, Vehicle vehicle);
    bool saveNode(unsigned long long gameTime, SReplayNode& node, Vehicle vehicle, bool firstNode);
    void finishRecord(bool saved, const SReplayNode& node);
    void clearPtfx();
    void createPtfx(const CTrackData& trackData);
    void clearTrackBlips();
    void createTrackBlips(const CTrackData& trackData);

    const CScriptSettings& mSettings;
    std::vector<std::shared_ptr<CReplayData>>& mReplays;
    std::vector<std::shared_ptr<CReplayData>> mCompatibleReplays;
    std::vector<CTrackData>& mTracks;
    std::vector<CImage>& mTrackImages;
    std::vector<CTrackData>& mArsTracks;

    CReplayData* mActiveReplay;
    CTrackData* mActiveTrack;

    std::unique_ptr<CWrappedBlip> mStartBlip;
    std::unique_ptr<CWrappedBlip> mFinishBlip;

    EScriptMode mScriptMode;
    ERecordState mRecordState;
    unsigned long long recordStart = 0;

    Vector3 mLastPos;

    std::unique_ptr<CReplayVehicle> mReplayVehicle;

    std::vector<CReplayData> mUnsavedRuns;
    CReplayData mCurrentRun;

    std::vector<int> mPtfxHandles;

    bool mPassengerModeActive = false;
    Vehicle mPassengerModePlayerVehicle;
    bool mPassengerModPlayerVehicleManagedByThisScript = false;
};
