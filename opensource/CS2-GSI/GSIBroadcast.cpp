#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <mutex>
#include <string>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")

static SOCKET listen_sock = INVALID_SOCKET;
static std::vector<SOCKET> clients;
static std::mutex client_mutex;

void FixPortReservation(int port) {
    // 构造 PowerShell 命令：停止 WinNAT -> 排除端口 -> 重启 WinNAT
    // 注意：这需要程序以管理员权限运行
    std::wstring cmd = L"powershell -Command \"Start-Process powershell -ArgumentList "
        L"'-Command \"net stop winnat; "
        L"netsh int ipv4 add excludedportrange protocol=tcp startport=" + std::to_wstring(port) +
        L" numberofports=1 store=persistent; "
        L"net start winnat\"' -Verb RunAs\"";

    std::wcout << L"[SYSTEM] 正在尝试修复端口 " << port << L" 的系统保留问题..." << std::endl;
    _wsystem(cmd.c_str());
}

void StartGSIBroadcast() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9001);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 在服务端代码里加上这个调试信息
    if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[SERVER ERROR] 绑定 9001 失败，错误码: %d\n", WSAGetLastError());
        FixPortReservation(9001);
    }
    else {
        printf("[SERVER OK] 9001 端口监听已开启，等待连接...\n");
    }
    listen(listen_sock, SOMAXCONN);

    std::thread([] {
        while (true) {
            SOCKET c = accept(listen_sock, nullptr, nullptr);
            if (c != INVALID_SOCKET) {
                std::lock_guard<std::mutex> lock(client_mutex);
                clients.push_back(c);
            }
        }
        }).detach();
}

void BroadcastGSI(const std::string& json) {
    std::lock_guard<std::mutex> lock(client_mutex);
    for (auto it = clients.begin(); it != clients.end(); ) {
        if (send(*it, json.c_str(), (int)json.size(), 0) <= 0 ||
            send(*it, "\n", 1, 0) <= 0) {
            closesocket(*it);
            it = clients.erase(it);
        }
        else {
            ++it;
        }
    }
}
