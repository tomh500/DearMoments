#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <thread>
#include "Tools.h"
#include "Global.h" 
#include <sstream>

bool isCS2Running = false;




void StartCS2() {
    // 关键点：使用 run 协议而非 rungameid，双斜杠后面接参数
    const wchar_t* steamUri = L"steam://run/730//-condebug/";

    if (debug == 1) {
        MessageBoxW(nullptr, L"正在拉起带参数的 CS2: -condebug", L"调试信息", MB_OK | MB_ICONINFORMATION);
    }

    HINSTANCE result = ShellExecuteW(nullptr, L"open", steamUri, nullptr, nullptr, SW_SHOWNORMAL);

    if ((int)result <= 32) {
        std::wstringstream ss;
        ss << L"无法通过 Steam 启动 CS2。\n错误代码：" << (int)result;
        MessageBoxW(nullptr, ss.str().c_str(), L"启动失败", MB_OK | MB_ICONERROR);
    }
}

void CheckCS2RunningAndUpdateButton(HWND hWnd) {
    // 1. 【初始环境检查】
    // 逻辑：程序一打开，发现 CS2 在跑但没 Hook
    if (IsProcessRunning(L"cs2.exe") && !IsProcessRunning(L"DearMouseHook.exe")) {
        int msgBoxID = MessageBoxW(hWnd,
            L"检测到 CS2 已在运行但必要服务未就绪！\n\n点击“确定”：强制关闭 CS2 并稍后手动开启服务。\n点击“取消”：暂不处理（但后续仍会被逻辑拦截）。",
            L"初始环境检查",
            MB_ICONWARNING | MB_OKCANCEL | MB_TOPMOST);

        if (msgBoxID == IDOK) {
            KillProcess(L"cs2.exe");
            isCS2Running = false;
        }
        else {
            // 如果点取消，我们把 isCS2Running 设为 true，
            // 这样进入循环后，它会被当成“掉线”逻辑杀掉，或者你可以直接在这里 ExitProcess
            isCS2Running = true;
        }
    }
    else {
        // 正常初始化状态
        isCS2Running = IsProcessRunning(L"cs2.exe");
    }

    // 2. 【持续监听循环】
   // 2. 【持续监听循环】
    while (true) {
        bool currentCS2Running = IsProcessRunning(L"cs2.exe");
        bool hookRunning = IsProcessRunning(L"DearMouseHook.exe");

        // 情况 1：发现 CS2 正在运行，但服务没开
        if (currentCS2Running && !hookRunning) {

            if (!isCS2Running) {
                // 【A. 拦截启动逻辑】：之前没在跑，现在突然冒出来的 -> 说明是在偷跑
                KillProcess(L"cs2.exe"); // 强杀偷跑的 CS2
                PostMessage(hWnd, WM_USER + 2, 0, 0); // 按钮改回“启动 CS2”
                MessageBoxW(hWnd, L"【拒绝启动】未运行必要服务时禁止启动 CS2！", L"安全防护", MB_ICONSTOP | MB_TOPMOST);
                currentCS2Running = false;
            }
            else {
                // 【B. 掉线警告逻辑】：之前在跑，现在服务突然没了 -> 仅仅警告
                // 不执行 KillProcess
                isCS2Running = false; // 标记为不再正常运行（同步状态）
                PostMessage(hWnd, WM_USER + 2, 0, 0); // 按钮改回“启动 CS2”

                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                MessageBoxW(hWnd,
                    L"【严重警告】CFG 必要服务已掉线！\n\n请注意：急停等核心功能已失效，请重新启动服务。",
                    L"服务状态异常",
                    MB_ICONERROR | MB_TOPMOST);
            }
            // 处理完异常后跳过本次循环
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // 情况 2：正常的 UI 状态同步（比如正常手动关闭游戏时，按钮要变回来）
        if (currentCS2Running != isCS2Running) {
            isCS2Running = currentCS2Running;
            PostMessage(hWnd, WM_USER + 2, 0, 0);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}