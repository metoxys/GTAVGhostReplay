#pragma once
#include "ReplayData.hpp"
#include "ReplayScript.hpp"
#include "ScriptMenu.hpp"
#include "ScriptSettings.hpp"

namespace GhostReplay {
    void ScriptMain();

    std::vector<CScriptMenu<CReplayScript>::CSubmenu> BuildMenu();

    CScriptSettings& GetSettings();

    CReplayScript* GetScript();

    uint32_t LoadReplays();
    uint32_t LoadTracks();
    uint32_t LoadTrackImages();
    uint32_t LoadARSTracks();

    void AddReplay(CReplayData replay);
}
