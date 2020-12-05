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
        return replayData;
    }

    replayFileStream >> replayJson;

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

    return replayData;
}

void CReplayData::Write() {
    nlohmann::json replayJson;

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
    std::ofstream replayFile(fmt::format("{}/{}.json", Constants::ModDir, Name));
    replayFile << std::setw(2) << replayJson << std::endl;
}
