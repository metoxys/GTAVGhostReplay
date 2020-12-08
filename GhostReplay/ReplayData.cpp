#include "ReplayData.hpp"
#include "Constants.hpp"
#include "Util/Paths.hpp"
#include "Util/Logger.hpp"
#include "ReplayScriptUtils.hpp"

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <utility>

void to_json(nlohmann::json& j, const Vector3& vector3) {
    j = nlohmann::json{
        { "X", vector3.x },
        { "Y", vector3.y },
        { "Z", vector3.z },
    };
}

void from_json(const nlohmann::json& j, Vector3& vector3) {
    j.at("X").get_to(vector3.x);
    j.at("Y").get_to(vector3.y);
    j.at("Z").get_to(vector3.z);
}

CReplayData CReplayData::Read(const std::string& replayFile) {
    CReplayData replayData(replayFile);

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
            if (jsonNode.find("PX") == jsonNode.end()) {
                node.Pos = jsonNode["Pos"];
                node.Rot = jsonNode["Rot"];
            }
            else {
                node.Pos.x = jsonNode["PX"];
                node.Pos.y = jsonNode["PY"];
                node.Pos.z = jsonNode["PZ"];
                node.Rot.x = jsonNode["RX"];
                node.Rot.y = jsonNode["RY"];
                node.Rot.z = jsonNode["RZ"];
            }
            node.WheelRotations = jsonNode.value("WheelRotations", std::vector<float>());
            node.SteeringAngle = jsonNode.value("Steering", 0.0f);
            node.Throttle = jsonNode.value("Throttle", 0.0f);
            node.Brake = jsonNode.value("Brake", 0.0f);
            node.Gear = jsonNode.value("Gear", -1);
            node.RPM = jsonNode.value("RPM", -1.0f);
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

CReplayData::CReplayData(std::string fileName)
    : MarkedForDeletion(false)
    , VehicleModel(0)
    , mFileName(std::move(fileName)) {}

void CReplayData::Write() {
    nlohmann::json replayJson;

    replayJson["Name"] = Name;
    replayJson["Track"] = Track;
    replayJson["VehicleModel"] = VehicleModel;
    replayJson["Mods"] = VehicleMods;

    for (auto& Node : Nodes) {
        replayJson["Nodes"].push_back({
            { "T", Node.Timestamp },
            { "Pos", Node.Pos },
            { "Rot", Node.Rot },
            { "WheelRotations", Node.WheelRotations },
            { "Steering", Node.SteeringAngle },
            { "Throttle", Node.Throttle },
            { "Brake", Node.Brake },
            { "Gear", Node.Gear },
            { "RPM", Node.RPM },
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
    mFileName = replayFileName;
}

void CReplayData::WriteAsync(const CReplayData& replayData) {
    std::thread([replayData]() {
        CReplayData myCopy = replayData;
        myCopy.Write();
    }).detach();
}

void CReplayData::Delete() const {
    std::filesystem::remove(std::filesystem::path(mFileName));
}
