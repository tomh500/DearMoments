#include "UpdateChecker.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <thread>
#include <iostream>
#include <wininet.h>
#include <shlwapi.h>
#include "Global.h"
#include <string>
#include "Resource.h"
#include "Tools.h"
#pragma comment(lib, "wininet.lib")
#include <filesystem>
using namespace std;
wchar_t title[256];
bool HotUpdate_req = false;

using namespace std;
using namespace filesystem;
namespace fs = std::filesystem;

const wchar_t* url;


void PerformHotUpdate(HWND hWnd, bool needsUpdate);
#include <sstream>

int ShowCloudMsg(HWND hWnd) {
    // 如果 regionCode 不等于 1，直接返回
    if (regionCode != 1) {
        return 0; // 空函数
    }

    // 定义消息文件的 URL
    const wchar_t* messageUrl = L"";

    // 打开网络连接
    HINTERNET hInternet = InternetOpenW(L"CloudMsgFetcher", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        MessageBoxW(hWnd, L"无法打开网络连接 (InternetOpenW 失败)", L"错误", MB_ICONERROR | MB_TASKMODAL);
        return -1;
    }

    // 打开远程 URL
    HINTERNET hMessageFile = InternetOpenUrlW(hInternet, messageUrl, NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hMessageFile) {
        MessageBoxW(hWnd, L"无法打开远程 Message.txt 文件 (InternetOpenUrlW 失败)", L"错误", MB_ICONERROR | MB_TASKMODAL);
        InternetCloseHandle(hInternet);
        return -1;
    }

    // 读取文件内容
    char messageBuffer[1024] = { 0 }; // 假定文件内容不会超过 1024 字节
    DWORD bytesRead = 0;
    if (!InternetReadFile(hMessageFile, messageBuffer, sizeof(messageBuffer) - 1, &bytesRead) || bytesRead == 0) {
        MessageBoxW(hWnd, L"无法读取远程 Message.txt 文件 (InternetReadFile 失败)", L"错误", MB_ICONERROR | MB_TASKMODAL);
        InternetCloseHandle(hMessageFile);
        InternetCloseHandle(hInternet);
        return -1;
    }

    // 确保缓冲区以空字符结尾
    messageBuffer[bytesRead] = '\0';

    // 解析第一行是否为 Enable=1;
    std::istringstream contentStream(messageBuffer);
    std::string firstLine;
    std::getline(contentStream, firstLine);
    if (firstLine != "Enable=1;") {
        // 如果第一行不是 Enable=1，直接退出
        InternetCloseHandle(hMessageFile);
        InternetCloseHandle(hInternet);
        return 0;
    }

    // 将内容转换为 wchar_t 并处理换行符
    std::string remainingContent;
    std::string line;
    while (std::getline(contentStream, line)) {
        remainingContent += line + "\n"; // 保留换行符
    }

    wchar_t wideMessageBuffer[1024] = { 0 };
    MultiByteToWideChar(CP_UTF8, 0, remainingContent.c_str(), -1, wideMessageBuffer, 1024);

    // 显示内容到 MessageBoxW (Tips 样式)
    MessageBoxW(hWnd, wideMessageBuffer, L"提示", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);

    // 清理网络资源
    InternetCloseHandle(hMessageFile);
    InternetCloseHandle(hInternet);

    return 0; // 返回成功状态
}
void CheckForUpdate(HWND hWnd) {
    std::thread([hWnd]() {
        std::wstring url;
        if (regionCode == 1) {
            url = L"";
        }
        else {
            url = L"";
        }

        if (debug == 1) {
            MessageBoxW(hWnd, L"[调试] 开始执行 CheckForUpdate", L"提示", MB_OK);
        }

        // === 打开网络 ===
        HINTERNET hInternet = InternetOpenW(L"UpdateChecker", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) {
            if (debug == 1) {
                MessageBoxW(hWnd, L"[调试] 无法打开网络连接 InternetOpenW 失败", L"错误", MB_ICONERROR);
            }
            return;
        }

        HINTERNET hFile = InternetOpenUrlW(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (!hFile) {
            if (debug == 1) {
                MessageBoxW(hWnd, L"[调试] 无法打开远程 URL InternetOpenUrlW 失败", L"错误", MB_ICONERROR);
            }
            InternetCloseHandle(hInternet);
            return;
        }

        // === 读取远程版本文件 ===
        char buffer[64] = { 0 };
        DWORD bytesRead = 0;
        if (!InternetReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead) || bytesRead == 0) {
            if (debug == 1) {
                MessageBoxW(hWnd, L"[调试] 无法读取远程版本文件 InternetReadFile 失败", L"错误", MB_ICONERROR);
            }
            InternetCloseHandle(hFile);
            InternetCloseHandle(hInternet);
            return;
        }
        buffer[bytesRead] = '\0';

        int remoteVersion = -1;
        int minRequiredVersion = -1;
        {
            std::string content(buffer);
            size_t commaPos = content.find(',');
            try {
                if (commaPos != std::string::npos) {
                    remoteVersion = std::stoi(content.substr(0, commaPos));
                    minRequiredVersion = std::stoi(content.substr(commaPos + 1));
                }
                else {
                    remoteVersion = std::stoi(content);
                    minRequiredVersion = -1;
                }
            }
            catch (...) {
                MessageBoxW(hWnd, L"远程版本文件格式错误", L"错误", MB_ICONERROR);
                InternetCloseHandle(hFile);
                InternetCloseHandle(hInternet);
                return;
            }
        }

        InternetCloseHandle(hFile);
        InternetCloseHandle(hInternet);

        // === 读取本地版本 ===
        
        fs::path absolutePath;
        try {
            fs::path absolutePath = RootPath / L"library"  / L"resource" / L"version.txt";

            std::ifstream localFile(absolutePath);
            if (!localFile.is_open()) {
                MessageBoxW(hWnd, (L"无法打开本地版本文件: " + absolutePath.wstring()).c_str(),
                    L"错误", MB_ICONERROR);
                return;
            }

            std::string line;
            if (!std::getline(localFile, line) || line.empty()) {
                MessageBoxW(hWnd, L"本地版本文件为空或无法读取", L"错误", MB_ICONERROR);
                return;
            }
            localVersion = std::stoi(line);
            localFile.close();
        }
        catch (const std::invalid_argument&) {
            MessageBoxW(hWnd, L"本地版本文件内容格式错误", L"错误", MB_ICONERROR);
            return;
        }
        catch (...) {
            MessageBoxW(hWnd, L"读取本地版本时发生未知异常", L"错误", MB_ICONERROR);
            return;
        }

        if (debug == 1) {
            wchar_t msg[256];
            swprintf_s(msg, 256, L"[调试] 本地版本=%d, 最新版本=%d, 强制最低版本=%d",
                localVersion, remoteVersion, minRequiredVersion);
            MessageBoxW(hWnd, msg, L"提示", MB_OK);
        }

        // === 版本逻辑 ===
        if (localVersion < remoteVersion) {
            HotUpdate_req = true;
            if (minRequiredVersion != -1 && localVersion < minRequiredVersion) {
                // 强制更新
                wchar_t msg[256];
                swprintf_s(msg, 256,
                    L"版本号过旧！\nYour version is %d.\n最新版本号: %d\n至少需要更新到: %d 才可使用。",
                    localVersion, remoteVersion, minRequiredVersion);
                MessageBoxW(hWnd, msg, L"错误 / Error", MB_ICONERROR | MB_OK);
                ExitProcess(0);
            }
            else {
                // 仅提示更新
                SetWindowTextW(hWnd, L"挚爱的时刻CFG-Listener（有新版本可用！）");
                    MessageBoxW(hWnd, L"[调试] 检测到新版本可用", L"提示", MB_ICONINFORMATION);
                    PerformHotUpdate(hWnd, true);

            }
        }
        else if (localVersion == remoteVersion) {
            SetWindowTextW(hWnd, L"挚爱的时刻CFG-Listener（最新版）");
            if (debug == 1) {
                MessageBoxW(hWnd, L"[调试] 是最新版", L"提示", MB_ICONINFORMATION);
            }
        }
        else {
            // 开发者 key
            fs::path devKeyPath = L"..\\..\\..\\dm.dev";
            if (!exists(devKeyPath)) {
                MessageBoxW(hWnd, L"版本号异常，请联系开发者\nVersion Error!", L"错误", MB_ICONERROR | MB_OK);
                ExitProcess(1);
            }
            else {
                MessageBoxW(hWnd, L"版本号异常，但您当前处于开发模式", L"警告", MB_ICONWARNING | MB_OK);
            }
        }
        }).detach();
}

#include <curl/curl.h>
#include <string>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

// 写入数据回调
static size_t WriteToString(void* ptr, size_t size, size_t nmemb, void* userdata) {
    std::string* str = static_cast<std::string*>(userdata);
    size_t totalSize = size * nmemb;
    str->append(static_cast<char*>(ptr), totalSize);
    return totalSize;
}

// 从 Windows 读取系统代理
static std::string GetSystemProxy() {
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig;
    if (WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig)) {
        if (proxyConfig.lpszProxy) {
            // 转换宽字符串为 UTF-8 窄字符串
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, proxyConfig.lpszProxy, -1, NULL, 0, NULL, NULL);
            std::string proxyUtf8(size_needed - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, proxyConfig.lpszProxy, -1, &proxyUtf8[0], size_needed, NULL, NULL);

            GlobalFree(proxyConfig.lpszProxy);
            if (proxyConfig.lpszProxyBypass) GlobalFree(proxyConfig.lpszProxyBypass);
            if (proxyConfig.lpszAutoConfigUrl) GlobalFree(proxyConfig.lpszAutoConfigUrl);
            return proxyUtf8;
        }
    }
    return "";
}

// 使用 curl 下载文本文件（自动应用系统代理）
std::string DownloadTextFile(const std::wstring& url, long timeoutSec = 15) {
    std::string buffer;
    CURL* curl = curl_easy_init();
    if (!curl) return buffer;

    // 宽字符串转 UTF-8 窄字符串
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, NULL, 0, NULL, NULL);
    std::string urlUtf8(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, url.c_str(), -1, &urlUtf8[0], size_needed, NULL, NULL);

    // 基本设置
    curl_easy_setopt(curl, CURLOPT_URL, urlUtf8.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

    // 检查系统代理
    std::string proxy = GetSystemProxy();
    if (!proxy.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
    }

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        buffer.clear();
    }
    return buffer;
}


// 热更新函数
void PerformHotUpdate(HWND hWnd, bool needsUpdate) {
    if (!needsUpdate) {
        // 如果版本已经是最新版，就直接空函数返回
        return;
    }

    MessageBoxW(hWnd, L"正在热更新，请稍候…", L"提示", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);

    // 选择热更新文件的 URL
    std::wstring url;
    if (regionCode == 1) {
        url = L"";
    }
    else {
        url = L"";
    }

    // 下载 HotUpdate.txt
    std::string hotUpdateContent = DownloadTextFile(url);
    if (hotUpdateContent.empty()) {
        MessageBoxW(hWnd, L"无法下载 HotUpdate.txt 文件，热更新失败。", L"错误", MB_ICONERROR | MB_TASKMODAL);
        return;
    }

    std::istringstream iss(hotUpdateContent);
    std::string line;
    std::string currentPath;

    while (std::getline(iss, line)) {
        // 去掉行首尾空格
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) continue;

        if (line[0] == '"') {
            size_t quoteEnd = line.find('"', 1);
            if (quoteEnd == std::string::npos) continue;

            std::string pathStr = line.substr(1, quoteEnd - 1);
            // 把所有的 \ 替换成 /，防止解析段落时出错
            std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
            currentPath = pathStr;
        }
        else if (line.rfind("-", 0) == 0 && !currentPath.empty()) {
            // 子项 URL，例如 - https://example.com/file.txt
            std::string url = line.substr(1);
            // 去空格
            url.erase(0, url.find_first_not_of(" \t"));
            url.erase(url.find_last_not_of(" \t\r\n") + 1);

            if (!url.empty()) {
                // 下载远程文件
                std::string fileData = DownloadTextFile(std::wstring(url.begin(), url.end()));
                if (fileData.empty()) {
                    if (debug == 1) {
                        MessageBoxW(hWnd, (L"下载失败: " + std::wstring(url.begin(), url.end())).c_str(),
                            L"错误", MB_ICONERROR);
                    }
                    continue;
                }

                // 构造目标路径
                fs::path targetPath = RootPath;
                std::istringstream pathStream(currentPath);
                std::string segment;
                while (std::getline(pathStream, segment, '/')) {
                    targetPath /= segment;
                }

                // 确保目录存在
                fs::create_directories(targetPath.parent_path());

                // 写入文件
                std::ofstream outFile(targetPath, std::ios::binary);
                if (!outFile) {
                    if (debug == 1) {
                        MessageBoxW(hWnd, (L"无法写入文件: " + targetPath.wstring()).c_str(),
                            L"错误", MB_ICONERROR);
                    }
                    continue;
                }
                outFile.write(fileData.data(), fileData.size());
                outFile.close();

                if (debug == 1) {
                    MessageBoxW(hWnd, (L"已更新文件: " + targetPath.wstring()).c_str(),
                        L"提示", MB_ICONINFORMATION);
                }
            }
        }
    }

    MessageBoxW(hWnd, L"热更新完成。", L"提示", MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
}