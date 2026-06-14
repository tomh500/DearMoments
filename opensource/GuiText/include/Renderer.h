#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <cmath>
#include "Global.h"

class Renderer {
public:
    void Initialize(HWND hWnd);
    void Render(const std::vector<std::wstring>& configLines);
    void HandleDPIChange(UINT newDPI);
    void Cleanup();
    void RenderSpecialText(HDC hdc, const std::wstring& text, int x, int y);
    void SetTextStyle(int style); // 设置文本样式
    void Update(); // 更新渐变状态

private:
    HFONT CreateCustomFont(int height, bool bold);
    COLORREF CalculateRainbowColor();

    HWND hWnd;
    HFONT hTitleFont;
    HFONT hConfigFont;
    COLORREF textColor = RGB(102, 204, 255);
    COLORREF specialTextColor = RGB(57, 197, 187);

    // RGB 渐变相关


    float hue = 0.0f; // 色调值 (0-360)
    DWORD lastUpdateTime = 0;
};