#include "Image.hpp"

#include "Util/Logger.hpp"
#include "Util/String.hpp"


#include <jpegsize/jpegsize.h>
#include <lodepng/lodepng.h>

#include <filesystem>


namespace fs = std::filesystem;

namespace {
    const std::vector<std::string> allowedExtensions{ ".png", ".jpg", ".jpeg" };
}

const std::vector<std::string>& Img::GetAllowedExtensions() {
    return allowedExtensions;
}

bool Img::GetIMGDimensions(std::string file, unsigned* width, unsigned* height) {
    auto ext = Util::to_lower(fs::path(file).extension().string());
    if (ext == ".png")
        return GetPNGDimensions(file, width, height);
    if (ext == ".jpg" || ext == ".jpeg")
        return GetJPGDimensions(file, width, height);
    return false;
}

bool Img::GetPNGDimensions(std::string file, unsigned* width, unsigned* height) {
    std::vector<unsigned char> image;
    unsigned error = lodepng::decode(image, *width, *height, file);
    if (error) {
        logger.Write(ERROR, "[Img] PNG error: %s: %s", std::to_string(error).c_str(), lodepng_error_text(error));
        return false;
    }
    return true;
}

bool Img::GetJPGDimensions(std::string file, unsigned* width, unsigned* height) {
    FILE* image = nullptr;

    errno_t err = fopen_s(&image, file.c_str(), "rb"); // open JPEG image file
    if (!image || err) {
        logger.Write(ERROR, "[Img] JPEG error: %s: Failed to open file", fs::path(file).filename().string().c_str());
        return false;
    }
    int w, h;
    int result = scanhead(image, &w, &h);
    if (result == 1) {
        *width = w;
        *height = h;
        return true;
    }
    logger.Write(ERROR, "[Img] JPEG error: %s: getting size failed.",
                 fs::path(file).filename().string().c_str());
    return false;
}
