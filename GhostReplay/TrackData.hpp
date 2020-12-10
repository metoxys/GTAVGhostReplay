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
    static CTrackData ReadARS(const std::string& trackFile);

    CTrackData(std::string fileName);
    void Write();
    void Delete() const;
    std::string FileName() const { return mFileName; }

    bool MarkedForDeletion;

    std::string Name;
    std::string Description;
    SLineDef StartLine;
    SLineDef FinishLine;
private:
    std::string mFileName;
};

