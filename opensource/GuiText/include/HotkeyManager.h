#pragma once
#include <windows.h>

class HotkeyManager {
public:
    void InstallHook(HWND hWnd);
    void UninstallHook();
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

private:
    static HWND hTargetWnd;
    static bool bVisible;
};