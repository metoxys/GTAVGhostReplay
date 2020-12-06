#include "ReplayData.hpp"
#include "Constants.hpp"
#include "Util/Paths.hpp"
#include "Util/Logger.hpp"
#include "Util/String.hpp"

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
        replayData.VehicleModel = replayJson["VehicleModel"];
        replayData.DriverModel = replayJson["DriverModel"];

        for (auto& jsonNode : replayJson["Nodes"]) {
            SReplayNode node{};
            node.Timestamp = jsonNode["T"];
            node.Pos.x = jsonNode["PX"];
            node.Pos.y = jsonNode["PY"];
            node.Pos.z = jsonNode["PZ"];
            node.Rot.x = jsonNode["RX"];
            node.Rot.y = jsonNode["RY"];
            node.Rot.z = jsonNode["RZ"];
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
    nlohmann::ordered_json replayJson;

    replayJson["Name"] = Name;
    replayJson["VehicleModel"] = VehicleModel;
    replayJson["DriverModel"] = DriverModel;

    for (auto& Node : Nodes) {
        replayJson["Nodes"].push_back({
            { "T", Node.Timestamp },
            { "PX", Node.Pos.x },
            { "PY", Node.Pos.y },
            { "PZ", Node.Pos.z },
            { "RX", Node.Rot.x },
            { "RY", Node.Rot.y },
            { "RZ", Node.Rot.z },
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
