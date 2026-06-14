#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>  // 包含滑块控件的功能
#include <gdiplus.h>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>

using namespace std;
using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib") // 确保连接公共控件库

// 全局变量
HWND hwndParent;  // 主窗口句柄
HWND hwndFileButton, hwndFileLabel, hwndSlider, hwndConvertButton;
wstring fileName;
float volumePercent = 100.0f;

// 调用FFmpeg的函数来改变音量并保存文件
void ConvertAudioWithFFmpeg(const wstring& inputFilePath, const wstring& outputFilePath, float volume) {
    wstringstream command;
    command << L"ffmpeg -i \"" << inputFilePath << L"\" -filter:a \"volume=" << volume / 100.0f << "\" \"" << outputFilePath << L"\"";
    wstring cmd = command.str();
    _wsystem(cmd.c_str());
}

// 删除旧文件并重命名临时文件
void ReplaceFile(const wstring& oldFile, const wstring& newFile) {
    DeleteFileW(oldFile.c_str());
    MoveFileW(newFile.c_str(), oldFile.c_str());
}

// 文件选择对话框
void ChooseFile(HWND hwnd) {
    OPENFILENAME ofn;       // common dialog box structure
    wchar_t szFile[260];    // buffer for file name

    // 初始化OPENFILENAME结构体
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Audio Files\0*.WAV;*.MP3;*.FLAC\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrTitle = L"Select an audio file";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // 显示文件选择对话框
    if (GetOpenFileName(&ofn) == TRUE) {
        fileName = szFile;
        SetWindowText(hwndFileLabel, fileName.c_str());
    }
}

// 附加窗口的窗口过程
LRESULT CALLBACK AttachWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // 创建按钮
        hwndFileButton = CreateWindow(L"BUTTON", L"Select File", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 20, 120, 40, hwnd, (HMENU)1, (HINSTANCE)GetWindowLong(hwnd, GWLP_HINSTANCE), NULL);
        hwndFileLabel = CreateWindow(L"STATIC", L"File: None", WS_VISIBLE | WS_CHILD,
            160, 20, 300, 40, hwnd, NULL, (HINSTANCE)GetWindowLong(hwnd, GWLP_HINSTANCE), NULL);

        // 创建音量滑块
        hwndSlider = CreateWindow(L"msctls_trackbar32", L"",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | TBS_AUTOTICKS | TBS_HORIZ,
            20, 80, 400, 30, hwnd, (HMENU)2, (HINSTANCE)GetWindowLong(hwnd, GWLP_HINSTANCE), NULL);
        SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 200));
        SendMessage(hwndSlider, TBM_SETPOS, TRUE, (LPARAM)100);

        // 创建转换按钮
        hwndConvertButton = CreateWindow(L"BUTTON", L"Convert", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 140, 120, 40, hwnd, (HMENU)3, (HINSTANCE)GetWindowLong(hwnd, GWLP_HINSTANCE), NULL);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1) {  // 选择文件按钮
            ChooseFile(hwnd);
        }
        else if (LOWORD(wParam) == 3) {  // 转换按钮
            if (!fileName.empty()) {
                int pos = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
                volumePercent = static_cast<float>(pos);
                wstring tempFile = fileName + L".tmp";

                // 创建新线程来调用FFmpeg
                std::thread([=]() {
                    ConvertAudioWithFFmpeg(fileName, tempFile, volumePercent);
                    ReplaceFile(fileName, tempFile);
                    MessageBox(hwnd, L"Conversion Complete!", L"Success", MB_OK);
                    }).detach();
            }
            else {
                MessageBox(hwnd, L"Please select a file first.", L"Error", MB_ICONERROR);
            }
        }
        break;
    }
    case WM_HSCROLL: {
        if ((HWND)lParam == hwndSlider) {
            int pos = (int)SendMessage(hwndSlider, TBM_GETPOS, 0, 0);
            volumePercent = static_cast<float>(pos);
            wstringstream label;
            label << L"File: " << fileName << L" (Volume: " << volumePercent << L"%)";
            SetWindowText(hwndFileLabel, label.str().c_str());
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// 主窗口与附加窗口的创建和显示
void CreateAttachWindow(HWND hwndParent) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = AttachWndProc;
    wc.hInstance = (HINSTANCE)GetWindowLong(hwndParent, GWLP_HINSTANCE);
    wc.lpszClassName = L"AttachWindowClass";
    RegisterClass(&wc);

    HWND hwndAttach = CreateWindowEx(0, wc.lpszClassName, L"Audio Volume Converter",
        WS_OVERLAPPEDWINDOW, 200, 200, 500, 250, hwndParent, NULL, (HINSTANCE)GetWindowLong(hwndParent, GWLP_HINSTANCE), NULL);

    // 设置父窗口为主窗口
    SetParent(hwndAttach, hwndParent);

    // 显示附加窗口
    ShowWindow(hwndAttach, SW_SHOWNORMAL);
    UpdateWindow(hwndAttach);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // 创建一个按钮来弹出附加窗口
        CreateWindow(L"BUTTON", L"Open Converter", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 20, 120, 40, hwnd, (HMENU)1, (HINSTANCE)GetWindowLong(hwnd, GWLP_HINSTANCE), NULL);
        break;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 1) {  // 点击“Open Converter”按钮，显示附加窗口
            CreateAttachWindow(hwnd);
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // 初始化GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // 初始化公共控件库
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    // 注册主窗口类
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MainWindowClass";
    RegisterClass(&wc);

    // 创建主窗口
    hwndParent = CreateWindowEx(0, wc.lpszClassName, L"Main Application Window", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 400, nullptr, nullptr, hInstance, nullptr);

    // 显示主窗口
    ShowWindow(hwndParent, nCmdShow);
    UpdateWindow(hwndParent);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 关闭GDI+
    GdiplusShutdown(gdiplusToken);

    return 0;
}
