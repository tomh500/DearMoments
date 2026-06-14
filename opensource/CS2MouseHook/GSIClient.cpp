#include "GSIClient.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <string>
#include <iostream>  
#include <mutex>
#include <chrono>


#pragma comment(lib, "ws2_32.lib")


// --- 全局逻辑变量 ---
static std::function<void(const std::string&)> g_callback;

// --- 新增：声明在 main.cpp 中定义的全局资源 ---
extern std::mutex g_output_mutex;    // 声明外部互斥锁
extern void GeiYanSe(WORD color);    // 声明外部函数
extern void ResetColor();            // 声明外部函数

bool is_gsi_connected = false;       // GSI 是否连接成功
bool degraded_mode_active = false;   // 是否处于降级（手动按键）模式
bool manual_allow_jiting = true;    // 降级模式下的急停许可


// 降级模式处理函数
void EnterDegradedMode() {
    if (degraded_mode_active) return; // 避免重复进入

    degraded_mode_active = true;
    std::lock_guard<std::mutex> lock(g_output_mutex);
    GeiYanSe(FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::wcout << L"\n[WARN] GSI 连接超时 (10s)！已切换至“手动按键降级模式”。" << std::endl;
    std::wcout << L"[INFO] 逻辑：1/2 开启急停，3/4/5/Q 禁用急停。" << std::endl;
    ResetColor();
}

// 恢复正常模式
void ExitDegradedMode() {
    degraded_mode_active = false;
    std::lock_guard<std::mutex> lock(g_output_mutex);
    GeiYanSe(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::wcout << L"\n[INFO] GSI 握手成功，已恢复自动武器识别模式。" << std::endl;
    ResetColor();
}

static void GSIHttpThread()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;

    while (true) // 最外层大循环，处理断线后的彻底重连
    {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) break;

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(9001);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        // --- 日志：开始尝试握手 ---
        {
            std::lock_guard<std::mutex> lock(g_output_mutex);
            GeiYanSe(FOREGROUND_RED | FOREGROUND_GREEN); // 黄色
            std::wcout << L"[NET ] 正在尝试连接音效包广播端 (Port: 9001)..." << std::endl;
            ResetColor();
        }

        if (connect(sock, (sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
            closesocket(sock);
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 连接失败，等2秒再试
            continue;
        }

        // --- 日志：握手成功 ---
        {
            std::lock_guard<std::mutex> lock(g_output_mutex);
            GeiYanSe(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            std::wcout << L"[NET ] [OK] 与广播端握手成功！开始监听指令流。" << std::endl;
            ResetColor();
        }

        std::string accumulator;
        char buf[4096];

        while (true)
        {
            int len = recv(sock, buf, sizeof(buf), 0);
            if (len > 0) {
                accumulator.append(buf, len);
                size_t pos;
                while ((pos = accumulator.find('\n')) != std::string::npos) {
                    std::string one_json = accumulator.substr(0, pos);
                    accumulator.erase(0, pos + 1);

                    if (!one_json.empty() && g_callback) {
                        g_callback(one_json);
                    }
                }
            }
            else {
                // --- 日志：连接断开 ---
                std::lock_guard<std::mutex> lock(g_output_mutex);
                GeiYanSe(FOREGROUND_RED | FOREGROUND_INTENSITY);
                std::wcout << L"[NET ] [ERR] 广播端已断开连接，准备尝试重连..." << std::endl;
                ResetColor();

                closesocket(sock);
                break; // 跳出 recv 循环，回到最外层尝试重连
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    WSACleanup();
}

void StartGSIListener(std::function<void(const std::string&)> onJson)
{
    g_callback = onJson;
    std::thread(GSIHttpThread).detach();
}