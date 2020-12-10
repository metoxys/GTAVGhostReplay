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

#include <menukeyboard.h>
#include <fmt/format.h>

namespace GhostReplay {
    std::vector<std::string> FormatTrackData(NativeMenu::Menu& mbCtx, CReplayScript& context, const CTrackData& track);
    bool EvaluateInput(std::string& searchFor);
    void UpdateTrackFilter(CReplayScript& context);

    bool trackSearchSelected = false;
    std::string trackSelectSearch;

    std::vector<CTrackData> filteredTracks;
    std::vector<CTrackData> filteredARSTracks;
}

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

        if (mbCtx.MenuOption(fmt::format("Track: {}", currentTrackName), "trackselectmenu")) {
            UpdateTrackFilter(context);
        }

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
            GhostReplay::LoadARSTracks();
            GhostReplay::LoadReplays();
            GhostReplay::LoadTrackImages();
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

        if (activeTrack && activeTrack->FileName() == "ARS") {
            mbCtx.Option(fmt::format("~r~ARS track: {}", currentTrackName),
                { "Tracks from ARS cannot be edited." });
            return;
        }

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

        if (context.GetTracks().empty() && context.GetARSTracks().empty()) {
            mbCtx.Option("No tracks");
            return;
        }

        if (trackSearchSelected) {
            if (EvaluateInput(trackSelectSearch)) {
                UpdateTrackFilter(context);
            }
        }

        std::vector<std::string> searchExtra {
            "Type to search. Use Delete for backspace.",
            "Searching for:",
            trackSelectSearch
        };
        if (mbCtx.OptionPlus("Search...", searchExtra, &trackSearchSelected, nullptr, nullptr, "Search")) {
            UpdateTrackFilter(context);
        }

        for (auto& track : filteredTracks) {
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
                auto extras = FormatTrackData(mbCtx, context, track);
                mbCtx.OptionPlusPlus(extras, track.Name);
            }
        }

        for (auto& track : filteredARSTracks) {
            bool currentTrack = false;
            if (context.ActiveTrack())
                currentTrack = context.ActiveTrack()->Name == track.Name;
            std::string selector = currentTrack ? "-> " : "";

            std::vector<std::string> description{
                "Press select to toggle track selection.",
            };

            std::string optionName = fmt::format("{}[ARS] {}", selector, track.Name);
            bool highlighted;
            if (mbCtx.OptionPlus(optionName, {}, &highlighted, nullptr, nullptr, "", description)) {
                if (!currentTrack)
                    context.SetTrack(track.Name);
                else
                    context.SetTrack("");
            }

            if (highlighted) {
                auto extras = FormatTrackData(mbCtx, context, track);
                mbCtx.OptionPlusPlus(extras, track.Name);
            }
        }

    });

    /* mainmenu -> ghostselectmenu */
    submenus.emplace_back("ghostselectmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Ghosts");

        if (!context.ActiveTrack()) {
            mbCtx.Subtitle("");
            mbCtx.Option("No track selected", { "A track needs to be selected before a ghost can be chosen." });
            return;
        }

        mbCtx.Subtitle(context.ActiveTrack()->Name);

        if (context.GetReplays().empty()) {
            mbCtx.Option("No ghosts");
            return;
        }

        for (auto& replay : context.GetReplays()) {
            bool currentReplay = false;
            if (context.ActiveReplay()) {
                currentReplay =
                    context.ActiveReplay()->Name == replay.Name &&
                    context.ActiveReplay()->Timestamp == replay.Timestamp;
            }
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

            std::string optionNameRaw = fmt::format("{} - {}",
                Util::FormatMillisTime(replay.Nodes.back().Timestamp),
                Util::GetVehicleName(replay.VehicleModel));
            std::string optionName = fmt::format("{}{}{}", deleteCol, selector, optionNameRaw);
            std::string datetime;
            if (replay.Timestamp == 0) {
                datetime = "No date";
            }
            else {
                datetime = Util::GetTimestampReadable(replay.Timestamp);
            }
            std::vector<std::string> extras
            {
                fmt::format("Track: {}", replay.Track),
                fmt::format("Car: {}", Util::GetVehicleName(replay.VehicleModel)),
                fmt::format("Time: {}",  Util::FormatMillisTime(replay.Nodes.back().Timestamp)),
                fmt::format("Lap recorded: {}", datetime),
            };

            if (mbCtx.OptionPlus(optionName, extras, nullptr, clearDeleteFlag, deleteFlag, "Ghost", description)) {
                if (!currentReplay)
                    context.SetReplay(replay.Name, replay.Timestamp);
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

            std::vector<std::string> ghostDetails = {
                "Select to save ghost.",
                fmt::format("Track: {}", unsavedRun.Track),
                fmt::format("Car: {}", Util::GetVehicleName(unsavedRun.VehicleModel)),
                fmt::format("Time: {}", Util::FormatMillisTime(unsavedRun.Nodes.back().Timestamp))
            };

            if (mbCtx.Option(fmt::format("{}", runName), ghostDetails)) {
                unsavedRun.Name = runName;

                CReplayData::WriteAsync(unsavedRun);
                GhostReplay::AddReplay(unsavedRun);
                context.AddCompatibleReplay(unsavedRun);

                unsavedRunIt = context.EraseUnsavedRun(unsavedRunIt);
                //GhostReplay::LoadReplays();
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

        mbCtx.BoolOption("Notify laptimes", GetSettings().Main.NotifyLaps,
            { "Show a notification when finishing a lap.",
              "Yellow: Normal",
              "Green: Faster than the ghost lap.",
              "Purple: All-time fastest lap." });

        mbCtx.MenuOption("Recording options", "recordoptionsmenu");
        mbCtx.MenuOption("Replay/ghost options", "replayoptionsmenu");
    });

    /* mainmenu -> settingsmenu -> recordoptionsmenu */
    submenus.emplace_back("recordoptionsmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Recording options");
        mbCtx.Subtitle("");

        mbCtx.BoolOption("Automatically save faster laps", GetSettings().Record.AutoGhost,
            { "Laps faster than the current playing ghost file, are saved to disk automatically." });

        std::vector<std::string> timestepDescr{
            "The minimum time in milliseconds between each ghost recording sample.",
            "The lower, the more accurate the ghost playback will be, but might increase file size "
                "and impact performance.",
        };
        int deltaMillis = GetSettings().Record.DeltaMillis;
        if (mbCtx.IntOptionCb("Recording timestep", deltaMillis, 0, 1000, 1, MenuUtils::GetKbInt, timestepDescr)) {
            GetSettings().Record.DeltaMillis = deltaMillis;
        }
    });

    /* mainmenu -> settingsmenu -> replayoptionsmenu */
    submenus.emplace_back("replayoptionsmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Replay options");
        mbCtx.Subtitle("");

        std::vector<std::string> replayAlphaDescr{
            "The transparency of the ghost vehicle."
            "0: completely invisible.",
            "255: completely visible.",
        };
        int replayAlpha = GetSettings().Replay.VehicleAlpha;
        if (mbCtx.IntOptionCb("Ghost alpha", replayAlpha, 0, 255, 1, MenuUtils::GetKbInt, replayAlphaDescr)) {
            GetSettings().Replay.VehicleAlpha = replayAlpha;
        }

        std::string fallbackOpt =
            fmt::format("Fallback vehicle model: {}", GetSettings().Replay.FallbackModel);
        std::vector<std::string> fallbackDescr{
            "When the vehicle model from a replay file is missing, replace it with this model."
        };
        if (mbCtx.Option(fallbackOpt, fallbackDescr)) {
            std::string value = GetSettings().Replay.FallbackModel;
            if (MenuUtils::GetKbString(value)) {
                GetSettings().Replay.FallbackModel = value;
            }
        }

        mbCtx.BoolOption("Force fallback model", GetSettings().Replay.ForceFallbackModel, 
            { "Always use the fallback model." });
    });

    return submenus;
}

std::vector<std::string> GhostReplay::FormatTrackData(NativeMenu::Menu& mbCtx, CReplayScript& context, const CTrackData& track) {
    auto compatibleReplays = context.GetCompatibleReplays(track.Name);
    CReplayData fastestReplayInfo = context.GetFastestReplay(track.Name, 0);
    std::string lapRecordString = "No times set";
    if (!fastestReplayInfo.Name.empty()) {
        lapRecordString = fmt::format("{} ({})",
            Util::FormatMillisTime(fastestReplayInfo.Nodes.back().Timestamp),
            Util::GetVehicleName(fastestReplayInfo.VehicleModel));
    }

    auto imgStr = context.GetTrackImageMenuString(track.Name);
    if (!imgStr.empty()) {
        imgStr = fmt::format("{}{}", mbCtx.ImagePrefix, imgStr);
    }
    else {
        imgStr = "No preview";
    }

    std::vector<std::string> extras{
        imgStr,
        fmt::format("Replays for this track: {}", compatibleReplays.size()),
        fmt::format("Lap record: {}", lapRecordString),
        fmt::format("Description: {}", track.Description),
    };

    std::sort(compatibleReplays.begin(), compatibleReplays.end(),
        [](const CReplayData& a, const CReplayData& b) {
            return a.Nodes.back().Timestamp < b.Nodes.back().Timestamp;
        });

    for (const auto& replay : compatibleReplays) {
        extras.push_back(fmt::format("{} ({})",
            Util::FormatMillisTime(replay.Nodes.back().Timestamp),
            Util::GetVehicleName(replay.VehicleModel)));
    }
    return extras;
}

bool GhostReplay::EvaluateInput(std::string& searchFor) {
    using namespace NativeMenu;
    HUD::SET_PAUSE_MENU_ACTIVE(false);
    PAD::DISABLE_ALL_CONTROL_ACTIONS(2);

    for (char c = ' '; c < '~'; c++) {
        int key = str2key(std::string(1, c));
        if (key == -1) continue;
        if (IsKeyJustUp(key)) {
            searchFor += c;
            return true;
        }
    }

    if ((IsKeyDown(str2key("LSHIFT")) || IsKeyDown(str2key("RSHIFT"))) && IsKeyJustUp(str2key("VK_OEM_MINUS"))) {
        searchFor += '_';
        return true;
    }
    if (IsKeyJustUp(str2key("VK_OEM_MINUS"))) {
        searchFor += '-';
        return true;
    }
    if (IsKeyJustUp(str2key("SPACE"))) {
        searchFor += ' ';
        return true;
    }
    if (IsKeyJustUp(str2key("DELETE")) && !searchFor.empty()) {
        searchFor.pop_back();
        return true;
    }

    return false;
}

void GhostReplay::UpdateTrackFilter(CReplayScript& context) {
    filteredTracks.clear();
    filteredARSTracks.clear();

    for (const auto& track : context.GetTracks()) {
        if (Util::FindSubstring(track.Name, trackSelectSearch) != -1 ||
            Util::FindSubstring(track.Description, trackSelectSearch) != -1) {
            filteredTracks.push_back(track);
        }
    }

    for (const auto& track : context.GetARSTracks()) {
        if (Util::FindSubstring(track.Name, trackSelectSearch) != -1 ||
            Util::FindSubstring(track.Description, trackSelectSearch) != -1) {
            filteredARSTracks.push_back(track);
        }
    }
}
