#include "HotkeyManager.h"
#include "Global.h"
// 静态成员变量定义
HWND HotkeyManager::hTargetWnd = nullptr;
bool HotkeyManager::bVisible = true;

void HotkeyManager::InstallHook(HWND hWnd) {
    hTargetWnd = hWnd;
    HHOOK hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!hHook) {
        MessageBox(NULL, L"安装键盘钩子失败!", L"错误", MB_ICONERROR);
    }
}

void HotkeyManager::UninstallHook() {
    // 钩子会在程序退出时自动卸载
}

LRESULT CALLBACK HotkeyManager::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKeyInfo = (KBDLLHOOKSTRUCT*)lParam;

        // 检查按键状态
        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);

        if (isKeyDown) {
            // '/'键 - 刷新配置
            if (pKeyInfo->vkCode == VK_OEM_2) {
                PostMessage(hTargetWnd, WM_USER + 1, 0, 0);
            }
            // INS键 - 切换显示
            else if (pKeyInfo->vkCode == VK_INSERT) {
                bVisible = !bVisible;
                PostMessage(hTargetWnd, WM_USER + 2, (WPARAM)bVisible, 0);
            }
        }
    }

    // 继续传递消息
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}