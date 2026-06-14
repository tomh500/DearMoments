#include "ProcessManager.h"

namespace ProcessManager {
    /**
     * @brief 根据进程名杀死进程
     * @param processName 进程映像名称 (例如 L"KICore.exe")
     * @return true 如果成功杀死了至少一个进程，false 则未找到或失败
     */
    bool KillProcess(const std::wstring& processName) {
        bool found = false;
        // 1. 创建系统进程快照
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        // 2. 遍历进程列表
        if (Process32FirstW(hSnap, &pe)) {
            do {
                // 比较进程名 (不区分大小写建议用 _wcsicmp)
                if (processName == pe.szExeFile) {
                    // 3. 打开进程句柄并尝试终止
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess != NULL) {
                        if (TerminateProcess(hProcess, 0)) {
                            found = true;
                        }
                        CloseHandle(hProcess);
                    }
                }
            } while (Process32NextW(hSnap, &pe));
        }

        CloseHandle(hSnap);
        return found;
    }
}