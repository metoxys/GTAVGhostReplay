#include "ScriptMenu.hpp"
#include "Script.hpp"
#include "ReplayScript.hpp"
#include "Constants.hpp"

#include "TrackData.hpp"
#include "ReplayData.hpp"

#include "ScriptMenuUtils.h"

#include "Util/UI.hpp"

#include <fmt/format.h>

std::vector<CScriptMenu<CReplayScript>::CSubmenu> GhostReplay::BuildMenu() {
    std::vector<CScriptMenu<CReplayScript>::CSubmenu> submenus;
    /* mainmenu */
    submenus.emplace_back("mainmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Ghost Car Replay");
        mbCtx.Subtitle(std::string("~b~") + Constants::DisplayVersion);

        CReplayData* activeReplay = context.ActiveReplay();
        CTrackData* activeTrack = context.ActiveTrack();

        if (context.ScriptMode() == EScriptMode::Idle) {
            std::string currentTrackName = "None";
            if (context.ActiveTrack())
                currentTrackName = context.ActiveTrack()->Name;

            std::string currentGhostName = "None";
            if (context.ActiveReplay())
                currentGhostName = context.ActiveReplay()->Name;

            mbCtx.MenuOption(fmt::format("Track: {}", currentTrackName), "trackselectmenu");
            mbCtx.MenuOption(fmt::format("Ghost: {}", currentTrackName), "ghostselectmenu");

        }
    });

    /* mainmenu -> trackselectmenu */
    submenus.emplace_back("trackselectmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Tracks");
        mbCtx.Subtitle("");

        for (const auto& track : context.GetTracks()) {
            bool currentTrack = false;
            if (context.ActiveTrack())
                currentTrack = context.ActiveTrack()->Name == track.Name;
            std::string selector = currentTrack ? "->" : "";

            std::string optionName = fmt::format("{}{}", selector, track.Name);
            if (mbCtx.Option(optionName)) {
                context.SetTrack(track.Name);
            }
        }
    });

    /* mainmenu -> ghostselectmenu */
    submenus.emplace_back("ghostselectmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Ghosts");
        mbCtx.Subtitle("");

        if (!context.ActiveTrack()) {
            mbCtx.Option("No track selected", { "A track needs to be selected before a ghost can be chosen." });
            return;
        }

        for (const auto& ghost : context.GetReplays()) {

        }
    });

    return submenus;
}
