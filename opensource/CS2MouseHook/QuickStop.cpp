

#include "QuickStop.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <map>
#include <algorithm>
#include <iostream>
#include "Global.h"
#include <windows.h>
#include <filesystem>

#include "yaml-cpp/yaml.h" // 假设你用 yaml-cpp

#pragma pack(push, 1)
struct SharedConfig {
    char profile_path[MAX_PATH]; // 固定长度，存路径字符串
    bool is_ready;               // 标志位
};
#pragma pack(pop)





namespace KICore {

    // --- 定义配置变量并设置默认值，防止 YAML 加载失败时程序崩溃 ---
    std::atomic<bool> ws_blocking(false), ad_blocking(false);
    std::atomic<bool> ignore_w{ false }, ignore_s{ false }, ignore_a{ false }, ignore_d{ false };

    bool is_holding_gun = true;
    std::vector<int> silent_keys = { VK_SHIFT, VK_CONTROL }; // 默认包含 shift 和 ctrl

    std::map<char, std::chrono::steady_clock::time_point> start_times;
    int g_min_pulse = 15;
    int g_max_pulse = 40;
    int g_cap_pulse = 42;
    int g_move_start_at = 100;
    int g_move_cap_at = 500;

    bool jiting = false;

    bool IsSilentKeyPressed() {
        for (int vk : silent_keys) {
            if (GetAsyncKeyState(vk) & 0x8000) return true;
        }
        return false;
    }

    void LoadConfigFromSharedMemory() {
        // 强制输出第一行，看看到底进没进来。注意：必须全用 wcout + L
        std::wcout << L"[DEBUG] 正在进入 LoadConfigFromSharedMemory..." << std::endl;

        HANDLE hMapFile = OpenFileMappingA(FILE_MAP_READ, FALSE, "KICore_Path_Share");
        if (!hMapFile) {
            // 报错也要用 wcout
            std::wcout << L"[ERROR] 找不到共享内存 KICore_Path_Share, 错误码: " << GetLastError() << std::endl;
            return;
        }

        SharedConfig* pBuf = (SharedConfig*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, sizeof(SharedConfig));
        if (pBuf) {
            // 检查 ready 信号
            if (!pBuf->is_ready) {
                std::wcout << L"[WARNING] 共享内存已连接，但主程序标记 is_ready 为 false" << std::endl;
            }
            else {
                // 将 char 数组路径转为 wstring 打印，防止崩溃
                std::string pPath(pBuf->profile_path);
                std::wstring wpPath = ToWString(pPath); // 使用你代码里已有的 ToWString 函数
                std::wcout << L"[DEBUG] 准备加载配置文件: " << wpPath << std::endl;

                std::filesystem::path configPath(pBuf->profile_path);
                if (std::filesystem::exists(configPath)) {
                    try {
                        YAML::Node config = YAML::LoadFile(configPath.string());
                        auto logic = config["quick_stop"]["logic"];

                        g_min_pulse = logic["min_pulse"].as<int>(15);
                        g_max_pulse = logic["max_pulse"].as<int>(40);
                        g_cap_pulse = logic["cap_pulse"].as<int>(42);
                        g_move_start_at = logic["move_start_at"].as<int>(100);
                        g_move_cap_at = logic["move_cap_at"].as<int>(500);

                        if (config["quick_stop"]["interception"]["silent_keys"]) {
                            KICore::silent_keys = config["quick_stop"]["interception"]["silent_keys"].as<std::vector<int>>();
                        }

                        std::wcout << L"[SUCCESS] YAML 配置加载成功！" << std::endl;
                    }
                    catch (const std::exception& e) {
                        // 关键：e.what() 是窄字符，不能直接给 wcout
                        std::wcout << L"[ERROR] YAML 解析标准异常" << std::endl;
                    }
                    catch (...) {
                        std::wcout << L"[ERROR] YAML 格式错误或未知异常" << std::endl;
                    }
                }
                else {
                    std::wcout << L"[ERROR] 磁盘上找不到文件: " << wpPath << std::endl;
                }
            }
            UnmapViewOfFile(pBuf);
        }
        CloseHandle(hMapFile);
    }
    void SendHardwareKey(WORD vKey, bool down) {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vKey;
        input.ki.wScan = (WORD)MapVirtualKey(vKey, MAPVK_VK_TO_VSC);
        input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | KEYEVENTF_SCANCODE;
        SendInput(1, &input, sizeof(INPUT));
    }

    void PerformDynamicStop(WORD vKey, int move_duration_ms, std::atomic<bool>* blocking_flag) {
        if (GetAsyncKeyState(vKey) & 0x8000) {
            blocking_flag->store(false);
            return;
        }

        // 使用配置变量进行线性计算
        int stop_pulse = g_min_pulse;
        if (move_duration_ms > g_move_start_at) {
            // 线性插值公式
            stop_pulse = g_min_pulse + (move_duration_ms - g_move_start_at) * (g_max_pulse - g_min_pulse) / (g_move_cap_at - g_move_start_at);
        }

        // 最终限幅
        if (stop_pulse > g_cap_pulse) stop_pulse = g_cap_pulse;

        // 执行模拟按键
        SendHardwareKey(vKey, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(stop_pulse));

        // 设置忽略旗标，防止反向按键触发自己的 start 事件
        if (vKey == 'W') ignore_w = true;
        else if (vKey == 'S') ignore_s = true;
        else if (vKey == 'A') ignore_a = true;
        else if (vKey == 'D') ignore_d = true;

        SendHardwareKey(vKey, false);

        // 阻塞保护期：防止连点抽风
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        blocking_flag->store(false);
    }

    // 内部文本解析：判断当前是不是拿着枪
    void UpdateWeaponState(const std::string& json) {
        if (json.find("\"weapons\"") == std::string::npos) return;

        // 1. 寻找当前手持的武器 (状态为 active 的那个)
        size_t activePos = json.find("\"state\": \"active\"");
        if (activePos == std::string::npos) return;

        // 2. 以 active 状态为基准，往回找这个武器的 type
        // 通常 type 会在 state 之前的 100 个字符以内
        size_t searchStart = (activePos > 150) ? activePos - 150 : 0;
        std::string weaponZone = json.substr(searchStart, 200);

        // 3. 只在这个 active 武器的范围内判断类型
        bool active_is_knife = (weaponZone.find("\"type\": \"Knife\"") != std::string::npos);
        bool active_is_nade = (weaponZone.find("\"type\": \"Grenade\"") != std::string::npos);
        bool active_is_c4 = (weaponZone.find("\"type\": \"C4\"") != std::string::npos);

        is_holding_gun = !(active_is_knife || active_is_nade || active_is_c4);

        // 调试打印：让你知道现在的判断结果
        // std::wcout << L"[GSI] 当前手持: " << (is_holding_gun ? L"【枪支】" : L"【刀雷】") << std::endl;
    }

    void ProcessQuickStopCommand(const std::string& cmd) {
        // A. 如果是 GSI JSON 数据
        if (cmd.find('{') != std::string::npos) {
            UpdateWeaponState(cmd);
            return;
        }

        // B. 处理移动起点的计时
        if (cmd.find("quickstart_") != std::string::npos) {
            char key = toupper(cmd.back());
            start_times[key] = std::chrono::steady_clock::now();
            return;
        }

        // C. 执行急停
        if (cmd.find("quickstop_") != std::string::npos) {
			if (g_pausejiting_ctrl) return;     //pause键控制的全局急停开关
            if (!jiting) return;               // 总开关
            if (IsSilentKeyPressed()) return;  // Shift/Ctrl 拦截
            if (!is_holding_gun) return;      // 刀/雷 拦截

            char key = toupper(cmd.back());
            WORD counterKey = 0;
            std::atomic<bool>* flag = nullptr;

            if (key == 'W') { if (ignore_w.exchange(false)) return; counterKey = 'S'; flag = &ws_blocking; }
            else if (key == 'S') { if (ignore_s.exchange(false)) return; counterKey = 'W'; flag = &ws_blocking; }
            else if (key == 'A') { if (ignore_a.exchange(false)) return; counterKey = 'D'; flag = &ad_blocking; }
            else if (key == 'D') { if (ignore_d.exchange(false)) return; counterKey = 'A'; flag = &ad_blocking; }

            if (counterKey && flag && !flag->exchange(true)) {
                auto now = std::chrono::steady_clock::now();
                int duration = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - start_times[key]).count();
                std::thread(PerformDynamicStop, counterKey, duration, flag).detach();
            }
        }
    }
}
