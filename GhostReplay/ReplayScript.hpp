#pragma once
#include "ScriptSettings.hpp"
#include "ReplayData.hpp"

#include "Memory/VehicleExtensions.hpp"

#include <vector>
#include <string>

class CReplayScript {
public:
    CReplayScript(
        CScriptSettings& settings,
        std::vector<CReplayData>& replays);
    virtual ~CReplayScript();
    virtual void Tick();

    CReplayData* ActiveReplay() {
        return mActiveReplay;
    }

protected:
    const CScriptSettings& mSettings;
    std::vector<CReplayData>& mReplays;

    CReplayData* mActiveReplay;
};
