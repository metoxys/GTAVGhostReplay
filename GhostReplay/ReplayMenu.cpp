#include "ScriptMenu.hpp"
#include "Script.hpp"
#include "ReplayScript.hpp"
#include "Constants.hpp"

#include "TrackData.hpp"
#include "ReplayData.hpp"

#include "ScriptMenuUtils.h"
#include "ReplayScriptUtils.hpp"

#include "Util/UI.hpp"
#include "Util/Math.hpp"
#include "Util/String.hpp"

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
        std::vector<std::string> ghostDetails;
        if (activeReplay) {
            currentGhostName = activeReplay->Name;
            ghostDetails = {
                fmt::format("Track: {}", activeReplay->Track),
                fmt::format("Car: {}", Util::GetVehicleName(activeReplay->VehicleModel)),
                fmt::format("Time: {}", Util::FormatMillisTime(activeReplay->Nodes.back().Timestamp))
            };
        }

        mbCtx.MenuOption(fmt::format("Track: {}", currentTrackName), "trackselectmenu");
        mbCtx.MenuOption(fmt::format("Ghost: {}", currentGhostName), "ghostselectmenu", ghostDetails);

        if (mbCtx.MenuOption("Track setup", "tracksetupmenu")) {
            context.SetScriptMode(EScriptMode::DefineTrack);
        }
        else {
            context.SetScriptMode(EScriptMode::ReplayActive);
        }

        mbCtx.MenuOption("Unsaved runs", "unsavedrunsmenu");

        if (mbCtx.Option("Refresh tracks and ghosts", 
            { "Refresh tracks and ghosts if they are changed outside the script." } )) {
            GhostReplay::LoadTracks();
            GhostReplay::LoadReplays();
            context.SetTrack("");
            UI::Notify("Tracks and replays refreshed", false);
        }

        mbCtx.MenuOption("Settings", "settingsmenu");
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
                    CTrackData newTrack("");
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

        for (auto& track : context.GetTracks()) {
            bool currentTrack = false;
            if (context.ActiveTrack())
                currentTrack = context.ActiveTrack()->Name == track.Name;
            std::string selector = currentTrack ? "-> " : "";
            std::string deleteCol = track.MarkedForDeletion ? "~r~" : "";

            std::vector<std::string> description{
                "Press select to toggle track selection.",
            };

            if (!track.MarkedForDeletion) {
                description.emplace_back("Press left arrow to delete.");
            }
            else {
                description.emplace_back("Press right arrow to unmark deletion.");
                description.emplace_back("~r~Press left to confirm deletion.");
                description.emplace_back("~r~~h~THIS IS PERMANENT");
            }

            auto clearDeleteFlag = [&]() {
                track.MarkedForDeletion = false;
            };
            auto deleteFlag = [&]() {
                if (track.MarkedForDeletion) {
                    context.DeleteTrack(track);
                }
                else {
                    track.MarkedForDeletion = true;
                }
            };

            std::string optionName = fmt::format("{}{}{}", deleteCol, selector, track.Name);
            bool highlighted;
            if (mbCtx.OptionPlus(optionName, {}, &highlighted, clearDeleteFlag, deleteFlag, "", description)) {
                if (!currentTrack)
                    context.SetTrack(track.Name);
                else
                    context.SetTrack("");
            }

            if (highlighted) {
                auto compatibleReplays = context.GetCompatibleReplays(track.Name);
                std::vector<std::string> extras{
                    fmt::format("Replays for this track: {}", compatibleReplays.size())
                };

                for (const auto& replay : compatibleReplays) {
                    extras.push_back(fmt::format("{}", replay.Name));
                    extras.push_back(fmt::format("    Car: {}", Util::GetVehicleName(replay.VehicleModel)));
                    extras.push_back(fmt::format("    Time: {}", Util::FormatMillisTime(replay.Nodes.back().Timestamp)));
                    extras.push_back(fmt::format("\n"));
                }
                mbCtx.OptionPlusPlus(extras, "Track");
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

        for (auto& replay : context.GetReplays()) {
            bool currentReplay = false;
            if (context.ActiveReplay())
                currentReplay = context.ActiveReplay()->Name == replay.Name;
            std::string selector = currentReplay ? "-> " : "";
            std::string deleteCol = replay.MarkedForDeletion ? "~r~" : "";

            std::vector<std::string> description{
                "Press select to toggle replay selection.",
            };

            if (!replay.MarkedForDeletion) {
                description.emplace_back("Press left arrow to delete.");
            }
            else {
                description.emplace_back("Press right arrow to unmark deletion.");
                description.emplace_back("~r~Press left to confirm deletion.");
                description.emplace_back("~r~~h~THIS IS PERMANENT");
            }

            auto clearDeleteFlag = [&]() {
                replay.MarkedForDeletion = false;
            };

            bool triggerBreak = false;
            auto deleteFlag = [&]() {
                if (replay.MarkedForDeletion) {
                    context.DeleteReplay(replay);
                    triggerBreak = true;
                }
                else {
                    replay.MarkedForDeletion = true;
                }
            };

            std::string optionName = fmt::format("{}{}{}", deleteCol, selector, replay.Name);
            std::vector<std::string> extras
            {
                fmt::format("Track: {}", replay.Track),
                fmt::format("Car: {}", Util::GetVehicleName(replay.VehicleModel)),
                fmt::format("Time: {}",  Util::FormatMillisTime(replay.Nodes.back().Timestamp))
            };

            if (mbCtx.OptionPlus(optionName, extras, nullptr, clearDeleteFlag, deleteFlag, "Ghost", description)) {
                if (!currentReplay)
                    context.SetReplay(replay.Name);
                else
                    context.SetReplay("");
            }
            if (triggerBreak)
                break;
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
            std::string runName = Util::FormatReplayName(
                unsavedRun.Nodes.back().Timestamp,
                unsavedRun.Track,
                Util::GetVehicleName(unsavedRun.VehicleModel));

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

    /* mainmenu -> settingsmenu */
    submenus.emplace_back("settingsmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Settings");
        mbCtx.Subtitle("");

        mbCtx.BoolOption("Lines visible", GhostReplay::GetSettings().Main.LinesVisible);
        mbCtx.BoolOption("Auto save faster ghost", GhostReplay::GetSettings().Main.AutoGhost);
        int deltaMillis = GhostReplay::GetSettings().Main.DeltaMillis;
        if (mbCtx.IntOptionCb("Time between recorded nodes", deltaMillis, 0, 1000, 1, MenuUtils::GetKbInt)) {
            GhostReplay::GetSettings().Main.DeltaMillis = deltaMillis;
        }

    });

    return submenus;
}
