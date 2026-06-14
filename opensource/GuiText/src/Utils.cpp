#include "Utils.h"
#include <vector>

namespace Utils {
    std::wstring UTF8ToWide(const std::string& utf8) {
        if (utf8.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        std::wstring wide(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], size);
        return wide;
    }

    std::string WideToUTF8(const std::wstring& wide) {
        if (wide.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
        std::string utf8(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &utf8[0], size, nullptr, nullptr);
        return utf8;
    }

    COLORREF ParseColor(long long colorValue) {
        int r = (colorValue / 1000000) % 1000;
        int g = (colorValue / 1000) % 1000;
        int b = colorValue % 1000;
        return RGB(r, g, b);
    }

    void GetScaledScreenSize(int& width, int& height) {
        width = GetSystemMetrics(SM_CXSCREEN);
        height = GetSystemMetrics(SM_CYSCREEN);
        HDC hdc = GetDC(nullptr);
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
        float scale = static_cast<float>(dpiX) / 96.0f;
        width = static_cast<int>(width);
        height = static_cast<int>(height);
    }
}