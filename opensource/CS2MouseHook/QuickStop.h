#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace KICore {
    // 基础配置变量
    extern  bool jiting;
    extern bool is_holding_gun;
    extern std::vector<int> silent_keys; // 包含 VK_SHIFT, VK_CONTROL 
    // 处理函数
    void ProcessQuickStopCommand(const std::string& cmd);

    // 初始化/辅助
    bool IsSilentKeyPressed();
    extern std::vector<int> silent_keys;
    void LoadConfigFromSharedMemory();
}