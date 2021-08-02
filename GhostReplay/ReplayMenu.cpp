#include "ScriptMenu.hpp"
#include "Script.hpp"
#include "ReplayScript.hpp"
#include "Constants.hpp"
#include "GitInfo.h"

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
    void UpdateReplayFilter(CReplayScript& context);

    bool trackSearchSelected = false;
    std::string trackSelectSearch;

    std::vector<CTrackData> filteredTracks;
    std::vector<CTrackData> filteredARSTracks;

    bool replaySearchSelected = false;
    std::string replaySelectSearch;

    std::vector<CReplayData> filteredReplays;
}

std::vector<CScriptMenu<CReplayScript>::CSubmenu> GhostReplay::BuildMenu() {
    std::vector<CScriptMenu<CReplayScript>::CSubmenu> submenus;
    /* mainmenu */
    submenus.emplace_back("mainmenu", [](NativeMenu::Menu& mbCtx, CReplayScript& context) {
        mbCtx.Title("Ghost Car");
        mbCtx.Subtitle(fmt::format("~b~{}{}", Constants::DisplayVersion, GIT_DIFF));

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
                fmt::format("Car: {} {}",
                    Util::GetVehicleMake(activeReplay->VehicleModel),
                    Util::GetVehicleName(activeReplay->VehicleModel)),
                fmt::format("Time: {}", Util::FormatMillisTime(activeReplay->Nodes.back().Timestamp))
            };
        }

        if (mbCtx.MenuOption(fmt::format("Track: {}", currentTrackName), "trackselectmenu")) {
            UpdateTrackFilter(context);
        }

        if (mbCtx.MenuOption(fmt::format("Ghost: {}", currentGhostName), "ghostselectmenu", ghostDetails)) {
            UpdateReplayFilter(context);
        }

        bool replaying = context.GetReplayState() == EReplayState::Playing;
        std::string replayAbortOption = replaying ? "Cancel playing ghost" : "~m~Cancel playing ghost";
        std::vector<std::string> replayAbortDetail;
        if (replaying) {
            replayAbortDetail = { "Currently replaying a ghost car.",
                "Want to re-do your lap, but not cross the finish line? Use this to stop and reset the ghost car." };
        }
        if (mbCtx.Option(replayAbortOption, replayAbortDetail)) {
            if (replaying)
                context.SetReplayState(EReplayState::Finished);
        }

        bool recording = context.GetRecordState() == ERecordState::Recording;
        std::string recordAbortOption = recording ? "Cancel recording" : "~m~Cancel recording";
        std::vector<std::string> recordAbortDetail;
        if (recording) {
            recordAbortDetail = { "Currently recording a ghost car.",
                "Not racing anymore? Use this to stop and discard the recording." };
        }
        if (mbCtx.Option(recordAbortOption, recordAbortDetail)) {
            if (recording)
                context.SetRecordState(ERecordState::Finished);
        }

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
        if (mbCtx.OptionPlus("Search...", searchExtra, &trackSearchSelected, nullptr, nullptr, "Search",
            { "Matches track name and description." })) {
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
                description.emplace_back("Press Left to delete.");
            }
            else {
                description.emplace_back("Press Right to unmark deletion.");
                description.emplace_back("~r~Press Left to confirm deletion.");
                description.emplace_back("~r~~h~THIS IS PERMANENT");
            }

            auto clearDeleteFlag = [&]() {
                track.MarkedForDeletion = false;
            };
            auto deleteFlag = [&]() {
                if (track.MarkedForDeletion) {
                    context.DeleteTrack(track);
                    UpdateTrackFilter(context);
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

            std::string optionName = fmt::format("{}{}", selector, track.Name);
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

        if (replaySearchSelected) {
            if (EvaluateInput(replaySelectSearch)) {
                UpdateReplayFilter(context);
            }
        }

        std::vector<std::string> searchExtra{
            "Type to search. Use Delete for backspace.",
            "Searching for:",
            replaySelectSearch
        };
        if (mbCtx.OptionPlus("Search...", searchExtra, &replaySearchSelected, nullptr, nullptr, "Search",
            { "Matches replay name, vehicle name, track time and recording time." })) {
            UpdateReplayFilter(context);
        }

        for (auto& replay : filteredReplays) {
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
                description.emplace_back("Press Left to delete.");
            }
            else {
                description.emplace_back("Press Right to unmark deletion.");
                description.emplace_back("~r~Press Left to confirm deletion.");
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
                    UpdateReplayFilter(context);
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
                fmt::format("Car: {} {}",
                    Util::GetVehicleMake(replay.VehicleModel),
                    Util::GetVehicleName(replay.VehicleModel)),
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

            std::string datetime;
            if (unsavedRun.Timestamp == 0) {
                datetime = "No date";
            }
            else {
                datetime = Util::GetTimestampReadable(unsavedRun.Timestamp);
            }

            std::vector<std::string> ghostDetails = {
                "Select to save ghost.",
                fmt::format("Track: {}", unsavedRun.Track),
                fmt::format("Car: {} {}",
                    Util::GetVehicleMake(unsavedRun.VehicleModel),
                    Util::GetVehicleName(unsavedRun.VehicleModel)),
                fmt::format("Time: {}", Util::FormatMillisTime(unsavedRun.Nodes.back().Timestamp)),
                fmt::format("Lap recorded: {}", datetime),
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

        mbCtx.BoolOption("Draw start/finish", GetSettings().Main.DrawStartFinish,
            { "Draws green flares for the track start, red flares for the track finish.",
              "Setting applies next time a track is selected." });

        mbCtx.BoolOption("Ghost blips", GetSettings().Main.GhostBlips,
            { "Draws blips for the ghost vehicle.",
              "Setting applies next time a ghost is started." });

        mbCtx.BoolOption("Extensive replay telemetry", GetSettings().Main.ExtensiveReplayTelemetry,
            { "Adds additional telemetry data to replay files.",
              "May impact performance." });

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

        mbCtx.BoolOption("Auto-load quickest ghost", GetSettings().Replay.AutoLoadGhost, 
            { "Automatically loads the quickest ghost lap when a track is selected, for that specific car model." });

        std::vector<std::string> replayAlphaDescr{
            "The transparency of the ghost vehicle. Applied on ghost lap start.",
            "0: completely invisible.",
            "100: completely visible.",
        };
        int replayAlpha = GetSettings().Replay.VehicleAlpha;
        if (mbCtx.IntOptionCb("Ghost opacity %", replayAlpha, 0, 100, 20, MenuUtils::GetKbInt, replayAlphaDescr)) {
            if (replayAlpha < 0)
                replayAlpha = 0;
            if (replayAlpha > 100)
                replayAlpha = 100;
            GetSettings().Replay.VehicleAlpha = replayAlpha;
            ENTITY::SET_ENTITY_ALPHA(
                context.GetReplayVehicle(),
                map(GetSettings().Replay.VehicleAlpha, 0, 100, 0, 255), false);
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
        fmt::format("Description: {}", track.Description),
        fmt::format("Total ghosts: {}", compatibleReplays.size()),
        fmt::format("Lap record: {}", lapRecordString),
        "Fastest ghost of each model:",
    };

    // compatibleReplays is ordered quickest to slowest.
    std::sort(compatibleReplays.begin(), compatibleReplays.end(),
        [](const CReplayData& a, const CReplayData& b) {
            return a.Nodes.back().Timestamp < b.Nodes.back().Timestamp;
        });

    std::vector<Hash> fastestModels;
    for (const auto& replay : compatibleReplays) {
        if (std::find(fastestModels.begin(), fastestModels.end(), replay.VehicleModel) != fastestModels.end())
            continue;
        extras.push_back(fmt::format("{} ({})",
            Util::FormatMillisTime(replay.Nodes.back().Timestamp),
            Util::GetVehicleName(replay.VehicleModel)));
        fastestModels.push_back(replay.VehicleModel);
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
    if (IsKeyJustUp(str2key("VK_OEM_COMMA"))) {
        searchFor += ',';
        return true;
    }
    if (IsKeyJustUp(str2key("VK_OEM_PERIOD"))) {
        searchFor += '.';
        return true;
    }
    if ((IsKeyDown(str2key("LSHIFT")) || IsKeyDown(str2key("RSHIFT"))) && IsKeyJustUp(str2key("VK_OEM_1"))) {
        searchFor += ':';
        return true;
    }
    if (IsKeyJustUp(str2key("VK_OEM_1"))) {
        searchFor += ';';
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

void GhostReplay::UpdateReplayFilter(CReplayScript& context) {
    filteredReplays.clear();

    if (context.ActiveTrack() == nullptr)
        return;

    for (const auto& replay : context.GetCompatibleReplays(context.ActiveTrack()->Name)) {
        std::string vehicleName = Util::GetVehicleName(replay.VehicleModel);
        std::string tracktime = Util::FormatMillisTime(replay.Nodes.back().Timestamp);
        std::string datetime;
        if (replay.Timestamp == 0) {
            datetime = "No date";
        }
        else {
            datetime = Util::GetTimestampReadable(replay.Timestamp);
        }
        if (Util::FindSubstring(replay.Name, replaySelectSearch) != -1 ||
            Util::FindSubstring(vehicleName, replaySelectSearch) != -1 ||
            Util::FindSubstring(tracktime, replaySelectSearch) != -1 ||
            Util::FindSubstring(datetime, replaySelectSearch) != -1) {
            filteredReplays.push_back(replay);
        }
    }
}
