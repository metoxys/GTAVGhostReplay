#pragma once
#include "ScriptSettings.hpp"
#include "ReplayData.hpp"
#include "TrackData.hpp"

#include "Memory/VehicleExtensions.hpp"

#include <vector>
#include <string>

enum class EScriptMode {
    DefineTrack,
    ReplayActive,
};

enum class EReplayState {
    Idle,
    Playing,
    Finished,
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
        std::vector<CTrackData>& tracks);
    virtual ~CReplayScript();
    virtual void Tick();

    const std::vector<CReplayData>& GetReplays() const {
        return mReplays;
    }

    const std::vector<CTrackData>& GetTracks() const {
        return mTracks;
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

    void SetTrack(const std::string& trackName);
    void SetReplay(const std::string& replayName);
    void ClearUnsavedRuns();
    std::vector<CReplayData>::const_iterator EraseUnsavedRun(std::vector<CReplayData>::const_iterator runIt);

    bool StartLineDef(SLineDef& lineDef, const std::string& lineName);

protected:
    void updateReplay();
    void updateTrackDefine();
    bool passedLineThisTick(SLineDef line, Vector3 oldPos, Vector3 newPos);
    void createReplayVehicle(Hash model, CReplayData* activeReplay, Vector3 pos);
    bool isFastestLap(const std::string& trackName, Hash vehicleModel, unsigned long long timestamp);

    const CScriptSettings& mSettings;
    std::vector<CReplayData>& mReplays;
    std::vector<CTrackData>& mTracks;

    CReplayData* mActiveReplay;
    CTrackData* mActiveTrack;

    EScriptMode mScriptMode;
    EReplayState mReplayState;
    ERecordState mRecordState;
    unsigned long long replayStart = 0;
    unsigned long long recordStart = 0;

    Vector3 mLastPos;
    std::vector<SReplayNode>::iterator mLastNode;

    Vehicle mReplayVehicle;

    std::vector<CReplayData> mUnsavedRuns;
    CReplayData mCurrentRun;
};
