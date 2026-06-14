#include "AntiHack.h"
#include "framework.h"
using namespace std;
bool isInsertLocked = false;

void CheckForCheatProcesses() {
    // 定义外挂程序的进程名列表（不区分大小写）
    vector<wstring> cheatProcesses = {
        L"cheatengine.exe",
        L"injector.exe",
        L"lghub.exe",
        L"aimware.exe"
    };

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            wstring processName = pe.szExeFile;

            for (const auto& cheat : cheatProcesses) {
                if (_wcsicmp(processName.c_str(), cheat.c_str()) == 0) {
                    wstring msg = L"存在\"外挂\": " + processName;
                    MessageBoxW(NULL, msg.c_str(), L"安全警告", MB_ICONERROR | MB_OK);
                    CloseHandle(hSnapshot);
                    exit(4000);
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}


bool LockInsertKey(HWND hwnd) {
    if (!isInsertLocked && RegisterHotKey(hwnd, 1, MOD_NOREPEAT, VK_INSERT)) {
        isInsertLocked = true;
        return true;
    }
    return false;
}

void UnlockInsertKey(HWND hwnd) {
    if (isInsertLocked) {
        UnregisterHotKey(hwnd, 1);
        isInsertLocked = false;
    }
}

void ToggleInsertLock(HWND hwnd) {
    if (isInsertLocked) {
        UnlockInsertKey(hwnd);
    }
    else {
        LockInsertKey(hwnd);
    }
}
