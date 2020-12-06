#pragma once
#include <string>

namespace MenuUtils {
    bool GetKbInt(int& val);
    bool GetKbFloat(float& val);
    bool GetKbString(std::string& val);
}