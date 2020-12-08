#pragma once
#include <string>
#include <utility>
#include <vector>

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
    const std::vector<std::string>& GetAllowedExtensions();
    bool GetIMGDimensions(std::string file, unsigned* width, unsigned* height);
    bool GetPNGDimensions(std::string file, unsigned* width, unsigned* height);
    bool GetJPGDimensions(std::string file, unsigned* width, unsigned* height);
}
