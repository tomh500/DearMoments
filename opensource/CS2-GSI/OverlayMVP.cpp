#include <windows.h>
#include <filesystem>
#include <iostream>
#include <atomic>
#include <thread>
extern bool low_memory;

static std::atomic<bool> g_running{ true };
static HWND g_hWnd = nullptr;
static HBITMAP g_hBitmap = nullptr;          // 运行时使用的位图
static HBITMAP g_hBitmapInMemory = nullptr;  // 预加载时保存的位图（非 low_memory 模式）

LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hWnd, &ps);
        if (g_hBitmap) {
            HDC memDC = CreateCompatibleDC(dc);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, g_hBitmap);

            BITMAP bm;
            GetObject(g_hBitmap, sizeof(bm), &bm);

            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            int x = sw / 2 - bm.bmWidth / 2;
            int y = sh / 4 - bm.bmHeight / 2;

            BitBlt(dc, x, y, bm.bmWidth, bm.bmHeight, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteDC(memDC);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

void InitOverlayMVP() {
    if (!low_memory) {
        auto bmpPath = std::filesystem::current_path() / L"Userspace" / L"CS2" / L"gsi" / L"images" / L"ShowMVP.bmp";
        g_hBitmapInMemory = (HBITMAP)LoadImageW(nullptr, bmpPath.c_str(), IMAGE_BITMAP, 0, 0,
            LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        if (!g_hBitmapInMemory) {
            std::wcerr << L"[InitOverlayMVP] 预加载失败，Error=" << GetLastError() << std::endl;
        }
    }
}

void OverlayThreadProc() {
    const wchar_t CLASS_NAME[] = L"MVPOverlayClass";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    // 根据 low_memory 选择加载方式
    if (low_memory) {
        auto bmpPath = std::filesystem::current_path() / L"Userspace" / L"CS2" / L"gsi" / L"images" / L"ShowMVP.bmp";
        g_hBitmap = (HBITMAP)LoadImageW(nullptr, bmpPath.c_str(), IMAGE_BITMAP, 0, 0,
            LR_LOADFROMFILE | LR_CREATEDIBSECTION);
        if (!g_hBitmap) {
            std::wcerr << L"[OverlayThreadProc] LoadImageW 失败，Error=" << GetLastError() << std::endl;
            return;
        }
    }
    else {
        g_hBitmap = g_hBitmapInMemory;
        if (!g_hBitmap) {
            std::wcerr << L"[OverlayThreadProc] 预加载位图为空，请先调用 InitOverlayMVP\n";
            return;
        }
    }

    g_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        CLASS_NAME, L"",
        WS_POPUP,
        0, 0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!g_hWnd) {
        std::wcerr << L"[OverlayThreadProc] CreateWindowExW 失败，Error=" << GetLastError() << std::endl;
        if (low_memory && g_hBitmap) {
            DeleteObject(g_hBitmap);
            g_hBitmap = nullptr;
        }
        return;
    }

    SetLayeredWindowAttributes(g_hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10);
    }

    if (low_memory && g_hBitmap) {
        DeleteObject(g_hBitmap);
        g_hBitmap = nullptr;
    }

    g_hWnd = nullptr;
    UnregisterClassW(CLASS_NAME, wc.hInstance);

    std::wcout << L"[Overlay] 线程已退出\n";
}

// 你可以提供显示和隐藏的接口
static std::thread overlayThread;

void ShowMvpOverlay() {
    if (overlayThread.joinable()) return; // 避免重复启动

    g_running = true;
    overlayThread = std::thread(OverlayThreadProc);
    // 可以加个等待窗口创建成功的机制，比如 Sleep(50)
    Sleep(50);
}

void HideShowMVP() {
    if (g_running) {
        g_running = false;
        if (g_hWnd) PostMessageW(g_hWnd, WM_CLOSE, 0, 0);
        if (overlayThread.joinable()) overlayThread.join();
    }
}
