#pragma once
#include <inc/types.h>
#include <string>

struct SLineDef {
    Vector3 A{};
    Vector3 B{};
};

class CTrackData {
public:
    static CTrackData Read(const std::string& trackFile);

    CTrackData();
    void Write();

    std::string Name;
    SLineDef StartLine;
    SLineDef FinishLine;
};

