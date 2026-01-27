#pragma once
#include <windows.h>

// 定义指令类型
enum ActionType {
    ACTION_DELAY = 0,       // 纯延迟
    ACTION_KEY_TAP,         // 键盘击键 (按下+抬起)
    ACTION_MOUSE_SCROLL,    // 鼠标滚轮
    ACTION_MOUSE_CLICK      // 鼠标点击 (左键/右键)
};

// 定义单步指令结构
struct MacroStep {
    ActionType type;        // 动作类型
    int value;              // 键码/滚动力度/点击类型
    double duration;        // 动作执行后的精确休眠时间 (ms)
};

// 全能配置包
struct MacroConfig {
    int triggerKey;         // 触发键
    MacroStep* steps;       // 指令数组指针
    int stepCount;          // 指令总数
};

// 导出函数
#ifdef MACRO_ENGINE_EXPORTS
#define MACRO_API extern "C" __declspec(dllexport)
#else
#define MACRO_API extern "C" __declspec(dllimport)
#endif

MACRO_API void CALLBACK RunMacroRuntime(const MacroConfig* cfg, bool* running);