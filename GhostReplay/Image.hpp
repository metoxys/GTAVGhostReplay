#pragma once
#include <string>
#include <utility>
#include <vector>
#include <optional>

class CImage {
public:
    CImage() : Name(), ResX(0), ResY(0), TextureID(0) {}
    CImage(int textureId, std::string name, uint16_t resX, uint16_t resY) :
        Name(std::move(name)),
        ResX(resX),
        ResY(resY),
        TextureID(textureId) { }

    std::string Name;
    uint16_t ResX;
    uint16_t ResY;
    int TextureID;
};

namespace Img {
    std::optional<std::pair<uint32_t, uint32_t>> GetIMGDimensions(const std::string& path);

    std::optional<std::pair<uint32_t, uint32_t>> GetPNGDimensions(const std::string& path);
    std::optional<std::pair<uint32_t, uint32_t>> GetJPGDimensions(const std::string& path);
    std::optional<std::pair<uint32_t, uint32_t>> GetWebPDimensions(const std::string& path);
}
