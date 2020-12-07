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

    CTrackData(std::string fileName);
    void Write();
    std::string FileName() const { return mFileName; }
    void Delete() const;

    bool MarkedForDeletion;

    std::string Name;
    SLineDef StartLine;
    SLineDef FinishLine;
private:
    std::string mFileName;
};

