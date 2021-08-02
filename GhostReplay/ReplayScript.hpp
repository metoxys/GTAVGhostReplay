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
    Finished,
};

class CReplayScript {
public:
    CReplayScript(
        CScriptSettings& settings,
        std::vector<CReplayData>& replays,
        std::vector<CTrackData>& tracks,
        std::vector<CImage>& trackImages,
        std::vector<CTrackData>& arsTracks);
    virtual ~CReplayScript();
    virtual void Tick();

    std::vector<CReplayData>& GetReplays() {
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

    void SetReplayState(EReplayState replayState) {
        if (mReplayVehicle)
            mReplayVehicle->SetReplayState(replayState);
    }

    ERecordState GetRecordState() {
        return mRecordState;
    }

    void SetRecordState(ERecordState recordState) {
        mRecordState = recordState;
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

    std::vector<CReplayData> GetCompatibleReplays(const std::string& trackName);
    void AddCompatibleReplay(const CReplayData& value);

    bool IsFastestLap(const std::string& trackName, Hash vehicleModel, unsigned long long timestamp);
    CReplayData GetFastestReplay(const std::string& trackName, Hash vehicleModel);

    bool StartLineDef(SLineDef& lineDef, const std::string& lineName);
    void DeleteTrack(const CTrackData& track);
    void DeleteReplay(const CReplayData& replay);

    std::string GetTrackImageMenuString(const std::string& trackName);

protected:
    void updateReplay();
    void updateRecord(unsigned long long gameTime, bool startPassedThisTick, bool finishPassedThisTick);
    void updateTrackDefine();
    bool passedLineThisTick(SLineDef line, Vector3 oldPos, Vector3 newPos);
    void clearPtfx();
    void createPtfx(const CTrackData& trackData);

    const CScriptSettings& mSettings;
    std::vector<CReplayData>& mReplays;
    std::vector<CReplayData> mCompatibleReplays;
    std::vector<CTrackData>& mTracks;
    std::vector<CImage>& mTrackImages;
    std::vector<CTrackData>& mArsTracks;

    CReplayData* mActiveReplay;
    CTrackData* mActiveTrack;

    EScriptMode mScriptMode;
    ERecordState mRecordState;
    unsigned long long recordStart = 0;

    Vector3 mLastPos;

    // TODO: Refactor replay vehicle logic to... somewhere else.
    //Vehicle mReplayVehicle;
    //std::unique_ptr<CWrappedBlip> mReplayVehicleBlip;

    std::unique_ptr<CReplayVehicle> mReplayVehicle;

    std::vector<CReplayData> mUnsavedRuns;
    CReplayData mCurrentRun;

    std::vector<int> mPtfxHandles;
};
