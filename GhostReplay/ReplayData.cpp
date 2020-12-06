#include "ReplayData.hpp"
#include "Constants.hpp"
#include "Util/Paths.hpp"
#include "Util/Logger.hpp"
#include "ReplayScriptUtils.hpp"

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>

CReplayData CReplayData::Read(const std::string& replayFile) {
    CReplayData replayData{};

    nlohmann::json replayJson;
    std::ifstream replayFileStream(replayFile.c_str());
    if (!replayFileStream.is_open()) {
        logger.Write(ERROR, "[Replay] Failed to open %s", replayFile.c_str());
        return replayData;
    }

    try {
        replayFileStream >> replayJson;

        replayData.Name = replayJson["Name"];
        replayData.Track = replayJson["Track"];
        replayData.VehicleModel = replayJson["VehicleModel"];
        replayData.VehicleMods = replayJson.value("Mods", VehicleModData());

        for (auto& jsonNode : replayJson["Nodes"]) {
            SReplayNode node{};
            node.Timestamp = jsonNode["T"];
            node.Pos.x = jsonNode["PX"];
            node.Pos.y = jsonNode["PY"];
            node.Pos.z = jsonNode["PZ"];
            node.Rot.x = jsonNode["RX"];
            node.Rot.y = jsonNode["RY"];
            node.Rot.z = jsonNode["RZ"];
            node.Throttle = jsonNode.value("Throttle", 0.0f);
            node.Brake = jsonNode.value("Brake", 0.0f);
            node.SteeringAngle = jsonNode.value("Steering", 0.0f);
            node.LowBeams = jsonNode.value("LowBeams", false);
            node.HighBeams = jsonNode.value("HighBeams", false);

            replayData.Nodes.push_back(node);
        }
        logger.Write(DEBUG, "[Replay] Parsed %s", replayFile.c_str());
        return replayData;
    }
    catch (std::exception& ex) {
        logger.Write(ERROR, "[Replay] Failed to open %s, exception: %s", replayFile.c_str(), ex.what());
        return replayData;
    }
}

void CReplayData::Write() {
    nlohmann::json replayJson;

    replayJson["Name"] = Name;
    replayJson["Track"] = Track;
    replayJson["VehicleModel"] = VehicleModel;
    replayJson["Mods"] = VehicleMods;

    for (auto& Node : Nodes) {
        replayJson["Nodes"].push_back({
            { "T", Node.Timestamp },
            { "PX", Node.Pos.x },
            { "PY", Node.Pos.y },
            { "PZ", Node.Pos.z },
            { "RX", Node.Rot.x },
            { "RY", Node.Rot.y },
            { "RZ", Node.Rot.z },
            { "Throttle", Node.Throttle },
            { "Brake", Node.Brake },
            { "Steering", Node.SteeringAngle },
            { "LowBeams", Node.LowBeams },
            { "HighBeams", Node.HighBeams },
        });
    }

    const std::string replaysPath =
        Paths::GetModuleFolder(Paths::GetOurModuleHandle()) +
        Constants::ModDir +
        "\\Replays";

    std::string cleanName = Util::StripString(Name);
    unsigned count = 0;
    std::string suffix;

    while (std::filesystem::exists(fmt::format("{}\\{}{}.json", replaysPath, cleanName, suffix))) {
        if (suffix.empty()) {
            suffix = "_0";
        }
        else {
            suffix = fmt::format("_{}", ++count);
        }
    }

    const std::string replayFileName = fmt::format("{}\\{}{}.json", replaysPath, cleanName, suffix);

    std::ofstream replayFile(replayFileName);
    replayFile << std::setw(2) << replayJson << std::endl;

    logger.Write(INFO, "[Replay] Written %s", replayFileName.c_str());
}
