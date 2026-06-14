#include "ConfigParser.h"
#include "Renderer.h"
#include "HotkeyManager.h"
#include "Utils.h"
#include "Global.h"
#include <windows.h>
#include <iostream>
#include <thread>

HWND g_hWnd = nullptr; // 全局保存窗口句柄


// 全局变量
ConfigParser configParser;
Renderer renderer;
HotkeyManager hotkeyManager;
int textstyle= 0;
// 渲染位置相关变量
int RenderPoint = 1; // 1=右上角, 2=左上角, 3=右下角, 4=左下角, 5=自定义
int CustomRenderPointX = 0;
int CustomRenderPointY = 0;

// 特殊文本颜色
COLORREF textColor = RGB(102, 204, 255); // 默认颜色

const wchar_t* WINDOW_CLASS = L"ConfigWatermarkClass";
const wchar_t* WINDOW_TITLE = L"Config Watermark";
HFONT hTitleFont = nullptr;
HFONT hConfigFont = nullptr;

// 窗口过程
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


// 注册窗口类
void RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = nullptr;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = WINDOW_CLASS;
    wcex.hIconSm = nullptr;
    RegisterClassExW(&wcex);
}

// 创建透明窗口
HWND CreateTransparentWindow(HINSTANCE hInstance) {
    // 获取屏幕尺寸
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // 窗口尺寸
    int windowWidth = screenWidth;
    int windowHeight = screenHeight;

    // 根据渲染位置设置窗口位置
    int windowX = 0, windowY = 0;

    switch (RenderPoint) {
        case 1: // 右上角
            windowX = screenWidth - windowWidth;
            windowY = 0;
            break;
        case 2: // 左上角
            windowX = 0;
            windowY = 0;
            break;
        case 3: // 右下角
            windowX = screenWidth - windowWidth;
            windowY = screenHeight - windowHeight;
            break;
        case 4: // 左下角
            windowX = 0;
            windowY = screenHeight - windowHeight;
            break;
        case 5: // 自定义
            windowX = CustomRenderPointX;
            windowY = CustomRenderPointY;
            break;
        default: // 默认右上角
            windowX = screenWidth - windowWidth;
            windowY = 0;
    }

    // 创建窗口
    HWND hWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        WINDOW_CLASS,
        WINDOW_TITLE,
        WS_POPUP,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (hWnd) {
        // 设置窗口透明度
        SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    }

    return hWnd;
}

// 窗口过程
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {

     case WM_USER + 1: // 刷新配置
            configParser.ReadConfigFile();
            InvalidateRect(hWnd, nullptr, TRUE);
            break;

        case WM_USER + 2: // 切换显示
            ShowWindow(hWnd, wParam ? SW_SHOW : SW_HIDE);
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // 设置背景透明
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textColor);

            // 获取窗口尺寸
            RECT rect;
            GetClientRect(hWnd, &rect);

            // 获取屏幕尺寸
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            int screenHeight = GetSystemMetrics(SM_CYSCREEN);

            // 计算标题位置
            int titleX = 0, titleY = 10;

            // 根据渲染位置模式设置标题位置
            switch (RenderPoint) {
                case 1: // 右上角
                    titleX = rect.right - 10;
                    break;
                case 2: // 左上角
                    titleX = 10;
                    break;
                case 3: // 右下角
                    titleX = rect.right - 10;
                    titleY = rect.bottom - 10;
                    break;
                case 4: // 左下角
                    titleX = 10;
                    titleY = rect.bottom - 10;
                    break;
                case 5: // 自定义
                    titleX = CustomRenderPointX;
                    titleY = CustomRenderPointY;
                    break;
                default: // 默认右上角
                    titleX = rect.right - 10;
            }

            // 绘制标题 (使用大号字体)
            SelectObject(hdc, hTitleFont);
            const wchar_t* title = L"DearNextgen";

            // 计算标题尺寸
            SIZE titleSize;
            GetTextExtentPoint32W(hdc, title, wcslen(title), &titleSize);

            // 调整位置（右上角需要减去宽度）
            if (RenderPoint == 1 || RenderPoint == 3) {
                titleX -= titleSize.cx;
            }

            // 绘制标题
            TextOutW(hdc, titleX, titleY, title, wcslen(title));

            // 绘制配置项 (使用小号字体)
            SelectObject(hdc, hConfigFont);
            int yPos = titleY + titleSize.cy + 20; // 标题下方开始，增加间距

            const std::vector<std::wstring>& configLines = configParser.GetConfigLines();
            for (const auto& line : configLines) {
        // 创建去星号版本用于计算宽度
        std::wstring cleanLine;
        for (wchar_t c : line) {
            if (c != L'*') cleanLine += c;
        }
        
        // 使用去星号文本计算尺寸
        SIZE textSize;
        GetTextExtentPoint32W(hdc, cleanLine.c_str(), cleanLine.length(), &textSize);

        // 计算文本位置
        int textX = 0;
        switch (RenderPoint) {
            case 1: // 右上角
                textX = rect.right - textSize.cx - 10;
                break;
            case 2: // 左上角
                textX = 10;
                break;
            case 3: // 右下角
                textX = rect.right - textSize.cx - 10;
                break;
            case 4: // 左下角
                textX = 10;
                break;
            case 5: // 自定义
                textX = CustomRenderPointX;
                break;
            default: // 默认右上角
                textX = rect.right - textSize.cx - 10;
        }

        // 渲染特殊格式文本（原样渲染，包含星号）
        renderer.RenderSpecialText(hdc, line, textX, yPos);

        // 增加行间距
        yPos += 30;
    }

    EndPaint(hWnd, &ps);
    break;
}
        case WM_DPICHANGED: {
            // DPI改变时调整窗口大小
            RECT* const prcNewWindow = (RECT*)lParam;
            SetWindowPos(hWnd, nullptr,
                         prcNewWindow->left, prcNewWindow->top,
                         prcNewWindow->right - prcNewWindow->left,
                         prcNewWindow->bottom - prcNewWindow->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);

            // 重新创建字体以适应新DPI
            if (hTitleFont) DeleteObject(hTitleFont);
            if (hConfigFont) DeleteObject(hConfigFont);

            LOGFONT lf = {};
            wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");

            // 根据DPI缩放调整字体大小
            UINT dpi = HIWORD(wParam);
            int titleSize = -MulDiv(26, dpi, 72);
            int configSize = -MulDiv(14, dpi, 72);

            // 标题字体
            lf.lfHeight = titleSize;
            lf.lfWeight = FW_BOLD;
            hTitleFont = CreateFontIndirect(&lf);

            // 配置项字体
            lf.lfHeight = configSize;
            lf.lfWeight = FW_NORMAL;
            hConfigFont = CreateFontIndirect(&lf);

            InvalidateRect(hWnd, nullptr, TRUE);
            break;
        }

        case WM_CREATE: {



            // 创建初始字体
            LOGFONT lf = {};
            wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");

            // 获取当前DPI
            HDC hdc = GetDC(hWnd);
            int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hWnd, hdc);

            // 根据DPI缩放字体
            int titleSize = -MulDiv(26, dpi, 72); // 从24增大到26
            int configSize = -MulDiv(14, dpi, 72);

            // 标题字体
            lf.lfHeight = titleSize;
            lf.lfWeight = FW_BOLD;
            hTitleFont = CreateFontIndirect(&lf);

            // 配置项字体
            lf.lfHeight = configSize;
            lf.lfWeight = FW_NORMAL;
            hConfigFont = CreateFontIndirect(&lf);

            // 初始化自定义渲染位置（右上角）
            int screenWidth = GetSystemMetrics(SM_CXSCREEN);
            CustomRenderPointX = screenWidth - 10;
            CustomRenderPointY = 10;
           SetTimer(hWnd, 1, 10000, NULL); // 每10秒触发一次

            break;
        }
    case WM_TIMER:
            if (wParam == 1) {
                //renderer.Update(); // 更新渐变状态
                SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0,
                                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            }
            break;
        case WM_DESTROY:
            if (hTitleFont) DeleteObject(hTitleFont);
            if (hConfigFont) DeleteObject(hConfigFont);
            KillTimer(hWnd, 1); // 销毁定时器
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 主函数
int main() {
    system("chcp 65001>nul");
    HINSTANCE hInstance = GetModuleHandle(nullptr); // 先声明 hInstance
    // 注册窗口类
    g_hWnd = CreateTransparentWindow(hInstance);

    RegisterWindowClass(hInstance);

    // 显示控制台窗口
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    std::cout << "DearMoments-X 配置查看器已启动" << std::endl;

    // 设置DPI感知
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 初始化配置
    configParser.ReadConfigFile();

    // 创建窗口 - 只定义一次 hWnd
    HWND hWnd = CreateTransparentWindow(hInstance);
    if (!hWnd) {
        std::cout << "创建窗口失败!" << std::endl;
        return 1;
    }

    // 安装键盘钩子（代替注册热键）
    hotkeyManager.InstallHook(hWnd);

    // 显示窗口
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    if (hTitleFont) DeleteObject(hTitleFont);
    if (hConfigFont) DeleteObject(hConfigFont);

    return 0;
}

extern "C" __declspec(dllexport) void SafeExitApplication() {
    if (g_hWnd) {
        PostMessage(g_hWnd, WM_CLOSE, 0, 0); // 安全触发 WM_DESTROY
    }
}
