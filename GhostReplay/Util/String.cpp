#include "String.hpp"

#include <fmt/format.h>
#include <sstream>
#include <algorithm>

namespace {
    template<typename Out>
    void split(const std::string& s, char delim, Out result) {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            *(result++) = item;
        }
    }

    const std::string illegalChars = "\\/:?\"<>|";
}

std::string Util::to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::vector<std::string> Util::split(const std::string& s, char delim) {
    std::vector<std::string> elems;
    ::split(s, delim, std::back_inserter(elems));
    return elems;
}

std::string Util::ByteArrayToString(uint8_t* byteArray, size_t length) {
    std::string instructionBytes;
    for (int i = 0; i < length; ++i) {
        char buff[4];
        snprintf(buff, 4, "%02X ", byteArray[i]);
        instructionBytes += buff;
    }
    return instructionBytes;
}

std::string Util::StripString(std::string input) {
    auto it = input.begin();
    for (it = input.begin(); it < input.end(); ++it) {
        bool found = illegalChars.find(*it) != std::string::npos;
        if (found) {
            *it = '.';
        }
    }
    return input;
}

std::string Util::FormatMillisTime(unsigned long long totalTime) {
    unsigned long long minutes, seconds, milliseconds;
    milliseconds = totalTime % 1000;
    seconds = (totalTime / 1000) % 60;
    minutes = (totalTime / 1000) / 60;
    return fmt::format("{}:{:02d}.{:03d}", minutes, seconds, milliseconds);
}
