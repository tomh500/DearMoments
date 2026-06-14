#include "CommandHandler.h"
#include <fstream>
#include <shlobj.h>
#include <windows.h>
#include <string>
#include <iostream>
#include "MusicHead.h"
// 全局变量声明
HANDLE hCmdStdinWrite = NULL;
HANDLE hCmdStdoutRead = NULL;
namespace fs = std::filesystem;

extern fs::path RootPath;
extern int debug;
extern bool BypassProxy;
extern void SetAutoStartViaBat(bool enable);

// --- 1. 之前漏掉的函数实现，加上它就不再报错了 ---
void ResetApplicationTrace(HWND hWnd) {
    // 1. 清除自启动
    SetAutoStartViaBat(false);

    // 2. 清除协议隐藏文件
    fs::path ruleFile = RootPath / L"scripts" / L".rule";
    if (fs::exists(ruleFile)) {
        SetFileAttributesW(ruleFile.c_str(), FILE_ATTRIBUTE_NORMAL);
        fs::remove(ruleFile);
    }

    // 3. 其他重置
    debug = 0;

    MessageBoxW(hWnd, L"所有痕迹已清理，程序下次启动将重新要求同意协议。", L"重置完成", MB_OK | MB_ICONINFORMATION);
}

// --- 2. CMD 交互逻辑 ---
DWORD WINAPI ReadPipeThread(LPVOID lpParam) {
    char buffer[1024];
    DWORD bytesRead;
    while (ReadFile(hCmdStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::cout << buffer;
        std::cout.flush();
    }
    return 0;
}

void InitConsoleSession() {
    if (hCmdStdinWrite) return;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hStdinRead, hStdoutWrite;

    if (!CreatePipe(&hCmdStdoutRead, &hStdoutWrite, &sa, 0)) return;
    if (!CreatePipe(&hStdinRead, &hCmdStdinWrite, &sa, 0)) return;

    SetHandleInformation(hCmdStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hCmdStdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    if (CreateProcessW(NULL, (LPWSTR)L"cmd.exe", NULL, NULL, TRUE, 0, NULL, RootPath.c_str(), &si, &pi)) {
        CloseHandle(CreateThread(NULL, 0, ReadPipeThread, NULL, 0, NULL));
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
}

void SendToConsole(std::wstring cmd) {
    InitConsoleSession();
    cmd += L"\r\n";
    int size_needed = WideCharToMultiByte(CP_ACP, 0, cmd.c_str(), (int)cmd.length(), NULL, 0, NULL, NULL);
    std::string strAsm(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, cmd.c_str(), (int)cmd.length(), &strAsm[0], size_needed, NULL, NULL);

    DWORD written;
    WriteFile(hCmdStdinWrite, strAsm.c_str(), (DWORD)strAsm.length(), &written, NULL);
}

static std::vector<std::wstring> SplitCmd(const std::wstring& s) {
    std::wstringstream wss(s);
    std::vector<std::wstring> parts;
    std::wstring item;
    while (wss >> item) {
        parts.push_back(item);
    }
    return parts;
}

enum class CmdPerm {
    USER,        // 普通用户
    USER_DEBUG,  // 需要 debug
    ENG          // 需要 dm.dev
};


// --- 3. 命令分发逻辑 ---
// --- 权限检查辅助函数 ---
bool HasPermission(CmdPerm perm, bool isDebug, bool isEng) {
    switch (perm) {
    case CmdPerm::USER:       return true;
    case CmdPerm::USER_DEBUG: return isDebug;
    case CmdPerm::ENG:        return isEng;
    default:                  return false;
    }
}

void HandleUserCommand(HWND hWnd, int editControlID) {
    fs::path devFlag = RootPath.parent_path() / L"dm.dev";
    bool isEng = fs::exists(devFlag);

    WCHAR cmdBuffer[512] = { 0 };
    HWND hEdit = GetDlgItem(hWnd, editControlID);
    GetWindowTextW(hEdit, cmdBuffer, 512);
    std::wstring cmdStr(cmdBuffer);

    if (cmdStr.empty()) return;

    auto parts = SplitCmd(cmdStr);
    if (parts.empty()) return;

    // --- 逻辑：定义每个指令需要的权限 ---

    // 1. /mus 系列
    if (parts[0] == L"/mus") {

        if (parts.size() >= 2 && parts[1] == L"check") {
            int sysRate = GetSystemSampleRate();
            wchar_t buf[256];
            swprintf_s(buf, L"系统当前采样率: %d Hz\n当前配置采样率: %d Hz\n\n是否立即同步并重载音频系统？",
                sysRate, g_SampleRate);

            if (MessageBoxW(hWnd, buf, L"采样率检测", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                g_SampleRate = sysRate;
                std::lock_guard<std::mutex> lock(g_audioMutex);
                ma_device_stop(&g_device);
                ma_device_uninit(&g_device);
                ma_device_config config = ma_device_config_init(ma_device_type_playback);
                config.playback.format = ma_format_f32;
                config.playback.channels = 2;
                config.sampleRate = g_SampleRate;
                config.dataCallback = data_callback;
                if (ma_device_init(NULL, &config, &g_device) != MA_SUCCESS) {
                    MessageBoxW(hWnd, L"音频系统重载失败", L"错误", MB_OK | MB_ICONERROR);
                }
                else {
                    // 读取并显示设备实际采样率（miniaudio 填充）
                    wchar_t info[128];
                    swprintf_s(info, L"音频系统已重载\n设备实际采样率: %d Hz", g_device.sampleRate);
                    if (g_isDecoderValid) ma_device_start(&g_device);
                    MessageBoxW(hWnd, info, L"成功", MB_OK | MB_ICONINFORMATION);
                }
            }
   
        }

        else if (parts.size() >= 3 && parts[1] == L"rate") {
            // 设置采样率：设为 USER 权限
            if (HasPermission(CmdPerm::USER, debug, isEng)) {
                g_SampleRate = _wtoi(parts[2].c_str());
                wchar_t buf[128];
                swprintf_s(buf, L"采样率已设置为 %d Hz\n（输入 /mus reload 生效）", g_SampleRate);
                MessageBoxW(hWnd, buf, L"设置成功", MB_OK | MB_ICONINFORMATION);
            }
        }


        else if (parts.size() >= 2 && parts[1] == L"reload") {
            // 重载音频：这种底层操作建议设为 USER_DEBUG 或 ENG
            if (HasPermission(CmdPerm::USER, debug, isEng)) {
                std::lock_guard<std::mutex> lock(g_audioMutex);
                ma_device_stop(&g_device);
                ma_device_uninit(&g_device);

                ma_device_config config = ma_device_config_init(ma_device_type_playback);
                config.playback.format = ma_format_f32;
                config.playback.channels = 2;
                config.sampleRate = g_SampleRate;
                config.dataCallback = data_callback;

                if (ma_device_init(NULL, &config, &g_device) != MA_SUCCESS) {
                    MessageBoxW(hWnd, L"音频系统重载失败", L"错误", MB_OK | MB_ICONERROR);
                }
                else {
                    if (g_isDecoderValid) ma_device_start(&g_device);
                    MessageBoxW(hWnd, L"音频系统已重载", L"成功", MB_OK | MB_ICONINFORMATION);
                }
            }
            else {
                MessageBoxW(hWnd, L"权限不足：请先开启 /debug 模式", L"拒绝访问", MB_OK | MB_ICONWARNING);
            }
        }
        else {
            MessageBoxW(hWnd, L"用法：\n/mus rate <int>\n/mus reload\n/mus check", L"帮助", MB_OK);
        }
    }
    // 2. /debug (切换权限的开关，设为 USER)
    else if (cmdStr == L"/debug") {
        debug = !debug;
        MessageBoxW(hWnd, debug ? L"Debug 模式已开启" : L"Debug 模式已关闭", L"状态", MB_OK);
    }
    // 3. /dos (危险操作，设为 ENG)
    else if (cmdStr.find(L"/dos ") == 0) {
        if (HasPermission(CmdPerm::ENG, debug, isEng)) {
            std::wstring rawCmd = cmdStr.substr(5);
            SendToConsole(rawCmd);
        }
        else {
            MessageBoxW(hWnd, L"权限不足：需要 ENG 模式 (dm.dev)", L"拒绝访问", MB_OK | MB_ICONSTOP);
        }
    }
    // 4. /reset (设为 USER_DEBUG)
    else if (cmdStr == L"/reset") {
        if (HasPermission(CmdPerm::USER_DEBUG, debug, isEng)) {
            ResetApplicationTrace(hWnd);
        }
    }
    // 5. /BypassProxy (设为 USER_DEBUG)
    else if (cmdStr.find(L"/BypassProxy ") == 0) {
        if (HasPermission(CmdPerm::USER_DEBUG, debug, isEng)) {
            BypassProxy = (cmdStr.substr(13) == L"1");
            MessageBoxW(hWnd, BypassProxy ? L"代理已绕过" : L"代理已恢复", L"设置", MB_OK);
        }
    }
    else {
        MessageBoxW(hWnd, L"未知命令！", L"错误", MB_OK | MB_ICONERROR);
    }

    SetWindowTextW(hEdit, L"");
}