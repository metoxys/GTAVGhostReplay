#include "TrackData.hpp"
#include "Constants.hpp"
#include "Util/Paths.hpp"
#include "Util/Logger.hpp"
#include "Util/String.hpp"

#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <filesystem>
#include <fstream>

CTrackData CTrackData::Read(const std::string& trackFile) {
    CTrackData trackData{};

    nlohmann::json trackJson;
    std::ifstream replayFileStream(trackFile.c_str());
    if (!replayFileStream.is_open()) {
        return trackData;
    }

    replayFileStream >> trackJson;

    trackData.Name = trackJson["Name"];

    trackData.StartLine.A.x = trackJson["StartLine"]["A"]["X"];
    trackData.StartLine.A.y = trackJson["StartLine"]["A"]["Y"];
    trackData.StartLine.A.z = trackJson["StartLine"]["A"]["Z"];

    trackData.StartLine.B.x = trackJson["StartLine"]["A"]["X"];
    trackData.StartLine.B.y = trackJson["StartLine"]["A"]["Y"];
    trackData.StartLine.B.z = trackJson["StartLine"]["A"]["Z"];

    trackData.FinishLine.A.x = trackJson["FinishLine"]["B"]["X"];
    trackData.FinishLine.A.y = trackJson["FinishLine"]["B"]["Y"];
    trackData.FinishLine.A.z = trackJson["FinishLine"]["B"]["Z"];

    trackData.FinishLine.B.x = trackJson["FinishLine"]["B"]["X"];
    trackData.FinishLine.B.y = trackJson["FinishLine"]["B"]["Y"];
    trackData.FinishLine.B.z = trackJson["FinishLine"]["B"]["Z"];

    return trackData;
}

void CTrackData::Write() {
    nlohmann::json trackJson;

    trackJson["Name"] = Name;
    trackJson["StartLine"]["A"]["X"] = StartLine.A.x;
    trackJson["StartLine"]["A"]["Y"] = StartLine.A.y;
    trackJson["StartLine"]["A"]["Z"] = StartLine.A.z;

    trackJson["StartLine"]["A"]["X"] = StartLine.B.x;
    trackJson["StartLine"]["A"]["Y"] = StartLine.B.y;
    trackJson["StartLine"]["A"]["Z"] = StartLine.B.z;

    trackJson["FinishLine"]["B"]["X"] = FinishLine.A.x;
    trackJson["FinishLine"]["B"]["Y"] = FinishLine.A.y;
    trackJson["FinishLine"]["B"]["Z"] = FinishLine.A.z;

    trackJson["FinishLine"]["B"]["X"] = FinishLine.B.x;
    trackJson["FinishLine"]["B"]["Y"] = FinishLine.B.y;
    trackJson["FinishLine"]["B"]["Z"] = FinishLine.B.z;

    std::ofstream trackFile(fmt::format("{}/{}.json", Constants::ModDir, Name));
    trackFile << std::setw(2) << trackJson << std::endl;
}
