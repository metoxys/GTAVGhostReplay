#pragma once
#include <inc/enums.h>
#include <inc/types.h>
#include <string>

class CWrappedBlip {
public:
    CWrappedBlip(Vector3 pos, eBlipSprite blip, std::string name, eBlipColor color);
    CWrappedBlip(Entity entity, eBlipSprite blip, std::string name, eBlipColor color, bool showHeading);
    ~CWrappedBlip();

    Vector3 GetPosition() const;
    void SetPosition(Vector3 coord);

    eBlipColor GetColor() const;
    void SetColor(eBlipColor color);

    int GetAlpha() const;
    void SetAlpha(int alpha);

    void SetRotation(int rotation);
    void SetScale(float scale);

    eBlipSprite GetSprite() const;
    void SetSprite(eBlipSprite sprite);

    Entity GetEntity() const;

    std::string GetName();
    void SetName(const std::string& name);

    void Delete();

    bool Exists() const;

    Blip GetHandle() const;

    void ShowHeading(bool b);

private:
    int mHandle;
    Entity mEntity;
    std::string mName;
};
