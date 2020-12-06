#pragma once
#include "ScriptSettings.hpp"
#include "ReplayData.hpp"
#include "TrackData.hpp"

#include "Memory/VehicleExtensions.hpp"

#include <vector>
#include <string>

enum class EScriptMode {
    Idle,
    DefineTrack,
    ReplayActive,
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

    bool StartLineDef(SLineDef& lineDef, const std::string& lineName);

protected:
    void updateReplay();
    void updateTrackDefine();

    const CScriptSettings& mSettings;
    std::vector<CReplayData>& mReplays;
    std::vector<CTrackData>& mTracks;

    CReplayData* mActiveReplay;
    CTrackData* mActiveTrack;

    EScriptMode mScriptMode;
};
