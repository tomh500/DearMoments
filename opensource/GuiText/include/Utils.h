#pragma once
#include <string>
#include <windows.h>

namespace Utils {
    std::wstring UTF8ToWide(const std::string& utf8);
    std::string WideToUTF8(const std::wstring& wide);
    COLORREF ParseColor(long long colorValue);
    void GetScaledScreenSize(int& width, int& height);
}