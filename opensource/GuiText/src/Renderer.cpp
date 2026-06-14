#include "Renderer.h"
#include "Utils.h"
#include <cwchar>
#include <algorithm>

void Renderer::SetTextStyle(int style) {
    textstyle= style;
    hue = 0.0f; // 重置色调
    lastUpdateTime = GetTickCount();
}

void Renderer::Update() {
    if (textstyle!= 1) return; // 只在渐变模式下更新

    DWORD currentTime = GetTickCount();
    DWORD elapsed = currentTime - lastUpdateTime;

    if (elapsed > 16) { // 约60FPS
        hue += 0.5f; // 调整速度
        if (hue >= 360.0f) hue -= 360.0f;
        lastUpdateTime = currentTime;

        // 请求重绘
        InvalidateRect(hWnd, NULL, FALSE);
    }
}

COLORREF Renderer::CalculateRainbowColor() {
    // HSV 转 RGB
    float s = 1.0f; // 饱和度
    float v = 1.0f; // 亮度

    float c = v * s;
    float x = c * (1 - fabs(fmod(hue / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (hue < 60) {
        r = c; g = x; b = 0;
    } else if (hue < 120) {
        r = x; g = c; b = 0;
    } else if (hue < 180) {
        r = 0; g = c; b = x;
    } else if (hue < 240) {
        r = 0; g = x; b = c;
    } else if (hue < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }

    return RGB((r + m) * 255, (g + m) * 255, (b + m) * 255);
}

void Renderer::Initialize(HWND hWnd) {
    this->hWnd = hWnd;
    HDC hdc = GetDC(hWnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hWnd, hdc);

    int titleSize = -MulDiv(26, dpi, 72);
    int configSize = -MulDiv(14, dpi, 72);

    hTitleFont = CreateCustomFont(titleSize, true);
    hConfigFont = CreateCustomFont(configSize, false);
}

HFONT Renderer::CreateCustomFont(int height, bool bold) {
    const wchar_t* preferredFont = L"等线 Bold";
    const wchar_t* fallbackFont = L"Microsoft YaHei UI";

    LOGFONT lf = {};
    lf.lfHeight = height;
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, preferredFont);

    HFONT hFont = CreateFontIndirect(&lf);
    if (hFont) {
        HDC hdc = GetDC(nullptr);
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        wchar_t actualFontName[LF_FACESIZE];
        GetTextFace(hdc, LF_FACESIZE, actualFontName);
        SelectObject(hdc, oldFont);
        ReleaseDC(nullptr, hdc);

        if (wcsstr(actualFontName, preferredFont) == nullptr) {
            DeleteObject(hFont);
            wcscpy_s(lf.lfFaceName, LF_FACESIZE, fallbackFont);
            hFont = CreateFontIndirect(&lf);
        }
    }
    return hFont;
}

void Renderer::Render(const std::vector<std::wstring>& configLines) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    SetBkMode(hdc, TRANSPARENT);
      // 根据文本样式设置颜色
        if (textstyle== 1) {
            // RGB 渐变模式
            SetTextColor(hdc, CalculateRainbowColor());
        } else {
            // 固定颜色模式
            SetTextColor(hdc, textColor);
        }

    RECT rect;
    GetClientRect(hWnd, &rect);

    // 渲染标题
    SelectObject(hdc, hTitleFont);
    const wchar_t* title = L"DearMoments-X";
    SIZE titleSize;
    GetTextExtentPoint32W(hdc, title, wcslen(title), &titleSize);
    int titleX = rect.right - titleSize.cx - 10;
    TextOutW(hdc, titleX, 10, title, wcslen(title));

    // 渲染配置项
    SelectObject(hdc, hConfigFont);
    int yPos = 10 + titleSize.cy + 20;

    for (const auto& line : configLines) {
        SIZE textSize;
        GetTextExtentPoint32W(hdc, line.c_str(), line.length(), &textSize);
        int textX = rect.right - textSize.cx - 10;
        RenderSpecialText(hdc, line, textX, yPos);
        yPos += 30;
    }

    EndPaint(hWnd, &ps);
}

void Renderer::RenderSpecialText(HDC hdc, const std::wstring& text, int x, int y) {

        std::wstring cleanText;
    for (wchar_t c : text) {
        if (c != L'*') cleanText += c; // 过滤星号
    }
    
// 使用去星号文本计算总宽度
    SIZE totalSize;
    GetTextExtentPoint32W(hdc, cleanText.c_str(), cleanText.length(), &totalSize);

    // 调整起始位置（确保右对齐）
    int startX = x;

    size_t start = text.find(L'*');
    if (start == std::wstring::npos) {
        TextOutW(hdc, startX, y, text.c_str(), text.length());
        return;
    }

    size_t end = text.find(L'*', start + 1);
    if (end == std::wstring::npos) {
        TextOutW(hdc, startX, y, text.c_str(), text.length());
        return;
    }

    // 渲染第一部分（普通文本）
    std::wstring part1 = text.substr(0, start);
    TextOutW(hdc, startX, y, part1.c_str(), part1.length());

    // 获取第一部分宽度
    SIZE size1;
    GetTextExtentPoint32W(hdc, part1.c_str(), part1.length(), &size1);
    int newX = startX + size1.cx;

    // 设置特殊颜色
    COLORREF oldColor;
        if (textstyle== 1) {
            // RGB 渐变模式 - 特殊文本使用固定颜色
            oldColor = SetTextColor(hdc, specialTextColor);
        } else {
            // 固定颜色模式 - 特殊文本使用特殊颜色
            oldColor = SetTextColor(hdc, specialTextColor);
        }

    // 渲染特殊部分（不包括星号）
    std::wstring special = text.substr(start + 1, end - start - 1);
    TextOutW(hdc, newX, y, special.c_str(), special.length());

    // 获取特殊部分宽度
    SIZE sizeSpecial;
    GetTextExtentPoint32W(hdc, special.c_str(), special.length(), &sizeSpecial);
    newX += sizeSpecial.cx;

    // 恢复原颜色
    SetTextColor(hdc, oldColor);

    // 渲染剩余部分
    if (end + 1 < text.length()) {
        std::wstring part2 = text.substr(end + 1);
        TextOutW(hdc, newX, y, part2.c_str(), part2.length());
    }
}

void Renderer::HandleDPIChange(UINT newDPI) {
    if (hTitleFont) DeleteObject(hTitleFont);
    if (hConfigFont) DeleteObject(hConfigFont);

    int titleSize = -MulDiv(26, newDPI, 72);
    int configSize = -MulDiv(14, newDPI, 72);

    hTitleFont = CreateCustomFont(titleSize, true);
    hConfigFont = CreateCustomFont(configSize, false);
}

void Renderer::Cleanup() {
    if (hTitleFont) DeleteObject(hTitleFont);
    if (hConfigFont) DeleteObject(hConfigFont);
}