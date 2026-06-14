#define SDL_MAIN_HANDLED
#include <ws2tcpip.h>
#include <winsock2.h>
#include <shlwapi.h>
#include "Global.h"
#include <nlohmann/json.hpp>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif


using namespace std;
namespace fs = filesystem;




bool SetWorkingDirectory(const std::wstring& path) {
    try {
        fs::path newDir;

        if (path.empty()) {
            // 如果没有传入路径，设置为程序所在目录
#ifdef _WIN32
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
                return false;
            }
            newDir = fs::path(exePath).parent_path();
#else
            // Linux 获取程序路径（/proc/self/exe 符号链接）
            char exePath[1024] = { 0 };
            ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
            if (count == -1) {
                return false;
            }
            exePath[count] = '\0';
            newDir = fs::path(exePath).parent_path();
#endif
        }
        else {
            // 传入了路径，先转换为绝对路径
            // 这里把wstring转成utf8 string，Linux下没wchar_t目录的概念
#ifdef _WIN32
            newDir = fs::path(path);
#else
            // 转换 wstring 到 utf8 string
            std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
            std::string utf8Path = conv.to_bytes(path);
            newDir = fs::path(utf8Path);
#endif
            newDir = fs::absolute(newDir);
        }

        // 设置工作目录
        fs::current_path(newDir);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to set working directory: " << e.what() << std::endl;
        return false;
    }
}

void RunBatchFile(const std::wstring& exePath, const std::wstring& arguments, bool showWindow) {
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOWNORMAL : SW_HIDE;

    // 获取目标 exe 的目录
    wchar_t exeDir[MAX_PATH] = { 0 };
    wcscpy_s(exeDir, exePath.c_str());
    PathRemoveFileSpecW(exeDir);  // 获取 exe 的目录

    // 设置工作目录
    if (!SetWorkingDirectory(exeDir)) {
        DWORD err = GetLastError();
        std::wstringstream ss;
        ss << L"无法设置工作目录！错误代码：" << err;
        MessageBoxW(nullptr, ss.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    // 拼接命令行
    std::wstring cmdLine = L"\"" + exePath + L"\" " + arguments;
    LPWSTR cmdLineBuffer = &cmdLine[0];

    // 可选调试信息（如果需要调试输出，可以自行临时加上）


    if (!CreateProcessW(nullptr, cmdLineBuffer, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        DWORD err = GetLastError();

        std::wstringstream ss;
        ss << L"无法启动程序！错误代码：" << err;
        MessageBoxW(nullptr, ss.str().c_str(), L"错误", MB_OK | MB_ICONERROR);
        return;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

