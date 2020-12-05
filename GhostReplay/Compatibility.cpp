#include "Compatibility.h"

#include <Windows.h>

#include "Util/Logger.hpp"

template <typename T>
T CheckAddr(HMODULE lib, const std::string& funcName)
{
    FARPROC func = GetProcAddress(lib, funcName.c_str());
    if (!func)
    {
        logger.Write(ERROR, "[Compat] Couldn't get function [%s]", funcName.c_str());
        return nullptr;
    }
    logger.Write(DEBUG, "[Compat] Found function [%s]", funcName.c_str());
    return reinterpret_cast<T>(func);
}

void Compatibility::Setup() {
}

void Compatibility::Release() {
}
