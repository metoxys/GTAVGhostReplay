#include "ScriptMenu.hpp"
#include "Script.hpp"
#include "ReplayScript.hpp"
#include "Constants.hpp"

#include "TrackData.hpp"
#include "ReplayData.hpp"

#include "ScriptMenuUtils.h"

#include "Util/UI.hpp"
#include "Util/Math.hpp"

#include <fmt/format.h>

std::vector<CScriptMenu<CReplayScript>::CSubmenu> GhostReplay::BuildMenu() {
    std::vector<CScriptMenu<CReplayScript>::CSubmenu> submenus;
    /* mainmenu */
    submenus.emplace_back("mainmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Ghost Car Replay");
        mbCtx.Subtitle(std::string("~b~") + Constants::DisplayVersion);

        CReplayData* activeReplay = context.ActiveReplay();
        CTrackData* activeTrack = context.ActiveTrack();
        auto scriptMode = context.ScriptMode();

        std::string currentTrackName = "None";
        if (activeTrack)
            currentTrackName = activeTrack->Name;

        std::string currentGhostName = "None";
        if (activeReplay)
            currentGhostName = activeReplay->Name;

        mbCtx.MenuOption(fmt::format("Track: {}", currentTrackName), "trackselectmenu");
        mbCtx.MenuOption(fmt::format("Ghost: {}", currentGhostName), "ghostselectmenu");

        if (mbCtx.MenuOption("Track setup", "tracksetupmenu")) {
            context.SetScriptMode(EScriptMode::DefineTrack);
        }
        else {
            context.SetScriptMode(EScriptMode::ReplayActive);
        }

        mbCtx.MenuOption("Unsaved runs", "unsavedrunsmenu");
    });

    /* mainmenu -> tracksetupmenu */
    submenus.emplace_back("tracksetupmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Track setup");
        mbCtx.Subtitle("");

        CTrackData* activeTrack = context.ActiveTrack();
        std::string currentTrackName = "None";
        if (activeTrack)
            currentTrackName = activeTrack->Name;

        if (mbCtx.Option(fmt::format("Track name: {}", currentTrackName))) {
            std::string newName = currentTrackName == "None" ? "" : currentTrackName;
            if (MenuUtils::GetKbString(newName)) {
                if (activeTrack) {
                    activeTrack->Name = newName;
                    UI::Notify(fmt::format("Renamed track to {}", newName), false);
                    GhostReplay::LoadTracks();
                    context.SetTrack(newName);
                }
                else {
                    CTrackData newTrack;
                    newTrack.Name = newName;
                    newTrack.Write();
                    GhostReplay::LoadTracks();
                    context.SetTrack(newName);
                    UI::Notify(fmt::format("Created new track: {}", newName), false);
                }
            }
        }

        if (activeTrack) {
            Vector3 emptyVec{};
            if (activeTrack->StartLine.A != emptyVec &&
                activeTrack->StartLine.B != emptyVec) {
                UI::DrawSphere(activeTrack->StartLine.A, 0.25f, 255, 255, 255, 255);
                UI::DrawSphere(activeTrack->StartLine.B, 0.25f, 255, 255, 255, 255);
                UI::DrawLine(activeTrack->StartLine.A, activeTrack->StartLine.B, 255, 255, 255, 255);
            }

            if (activeTrack->FinishLine.A != emptyVec &&
                activeTrack->FinishLine.B != emptyVec) {
                UI::DrawSphere(activeTrack->FinishLine.A, 0.25f, 255, 255, 255, 255);
                UI::DrawSphere(activeTrack->FinishLine.B, 0.25f, 255, 255, 255, 255);
                UI::DrawLine(activeTrack->FinishLine.A, activeTrack->FinishLine.B, 255, 255, 255, 255);
            }

            if (mbCtx.Option("Set start line")) {
                SLineDef newLineDef{};
                if (context.StartLineDef(newLineDef, "start")) {
                    activeTrack->StartLine = newLineDef;
                }
            }

            if (mbCtx.Option("Set finish line")) {
                SLineDef newLineDef{};
                if (context.StartLineDef(newLineDef, "finish")) {
                    activeTrack->FinishLine = newLineDef;
                }
            }

            if (mbCtx.Option("Save track")) {
                std::string missingPoints;

                if (activeTrack->StartLine.A == emptyVec)
                    missingPoints = fmt::format("{}, {}", missingPoints, "Start left");
                if (activeTrack->StartLine.B == emptyVec)
                    missingPoints = fmt::format("{}, {}", missingPoints, "Start right");
                if (activeTrack->FinishLine.A == emptyVec)
                    missingPoints = fmt::format("{}, {}", missingPoints, "Finish left");
                if (activeTrack->FinishLine.B == emptyVec)
                    missingPoints = fmt::format("{}, {}", missingPoints, "Finish right");

                if (!missingPoints.empty()) {
                    UI::Notify(fmt::format("Couldn't save track. Missing points: {}", missingPoints), true);
                    return;
                }

                activeTrack->Write();
                UI::Notify(fmt::format("Saved track {}", activeTrack->Name), true);
                GhostReplay::LoadTracks();
            }
        }
    });

    /* mainmenu -> trackselectmenu */
    submenus.emplace_back("trackselectmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Tracks");
        mbCtx.Subtitle("");

        if (context.GetTracks().empty()) {
            mbCtx.Option("No tracks");
            return;
        }

        for (const auto& track : context.GetTracks()) {
            bool currentTrack = false;
            if (context.ActiveTrack())
                currentTrack = context.ActiveTrack()->Name == track.Name;
            std::string selector = currentTrack ? "> " : "";

            std::string optionName = fmt::format("{}{}", selector, track.Name);
            if (mbCtx.Option(optionName)) {
                if (!currentTrack)
                    context.SetTrack(track.Name);
                else
                    context.SetTrack("");
            }
        }
    });

    /* mainmenu -> ghostselectmenu */
    submenus.emplace_back("ghostselectmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Ghosts");
        mbCtx.Subtitle("");

        if (context.GetReplays().empty()) {
            mbCtx.Option("No ghosts");
            return;
        }

        if (!context.ActiveTrack()) {
            mbCtx.Option("No track selected", { "A track needs to be selected before a ghost can be chosen." });
            return;
        }

        for (const auto& replay : context.GetReplays()) {
            bool currentReplay = false;
            if (context.ActiveReplay())
                currentReplay = context.ActiveReplay()->Name == replay.Name;
            std::string selector = currentReplay ? "->" : "";

            std::string optionName = fmt::format("{}{}", selector, replay.Name);
            if (mbCtx.Option(optionName)) {
                if (!currentReplay)
                    context.SetReplay(replay.Name);
                else
                    context.SetReplay("");
            }
        }
    });

    /* mainmenu -> unsavedrunsmenu */
    submenus.emplace_back("unsavedrunsmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Unsaved runs");
        mbCtx.Subtitle("");

        if (context.GetUnsavedRuns().empty()) {
            mbCtx.Option("No unsaved runs");
            return;
        }

        auto unsavedRunIt = context.GetUnsavedRuns().begin();
        while (unsavedRunIt != context.GetUnsavedRuns().end()) {
            auto unsavedRun = *unsavedRunIt;
            unsigned long long minutes, seconds, milliseconds;
            auto totalTime = unsavedRun.Nodes.back().Timestamp;
            milliseconds = totalTime % 1000;
            seconds = (totalTime / 1000) % 60;
            minutes = (totalTime / 1000) / 60;
            std::string timeStr = fmt::format("{}:{:02d}.{:03d}", minutes, seconds, milliseconds);

            std::string trackName = unsavedRun.Track;
            std::string runName = fmt::format("{} - {}", trackName, timeStr);

            if (mbCtx.Option(fmt::format("Save [{}]", runName))) {
                unsavedRun.Name = runName;
                unsavedRun.Write();
                unsavedRunIt = context.EraseUnsavedRun(unsavedRunIt);
                GhostReplay::LoadReplays();
            }
            else {
                ++unsavedRunIt;
            }
        }

        if (mbCtx.Option("Clear unsaved runs")) {
            context.ClearUnsavedRuns();
        }
    });

    return submenus;
}
