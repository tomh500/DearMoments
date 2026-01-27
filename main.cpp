#include <iostream>
#include <vector>
#include <windows.h>
#include "MacroCommon.h"

// 链接 DLL 导入库和多媒体计时器库
#pragma comment(lib, "DearMacro_.lib")
#pragma comment(lib, "winmm.lib")

// ======================== 躯体：用户宏逻辑编写区 ========================
// 网页生成的代码将直接粘贴或填充到这个命名空间中
namespace UserMacro {
    // 基础配置
    int TRIGGER_KEY = VK_SPACE;
    int SCROLL_DOWN = -120; // 滚轮信号强度
    double GAP = 20.0;      // 组间停顿

    // 1:1 移植原脚本的三组数据，转化为 DLL 引擎识别的指令流
    // 每个指令包含：{ 动作类型, 键码/数值, 动作后休眠时间 }
    MacroStep mySteps[] = {
        // --- 第一组 (DATA_1) ---
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 37.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 16.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 17.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 12.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 15.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 14.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 21.0 },
        { ACTION_DELAY,        0,           GAP  }, // 组间停顿

        // --- 第二组 (DATA_2) ---
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 19.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 24.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 16.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 23.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 16.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 20.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 19.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 31.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 36.0 },
        { ACTION_DELAY,        0,           GAP  }, // 组间停顿

        // --- 第三组 (DATA_3) ---
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 40.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 30.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 30.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 19.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 21.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 14.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 19.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 16.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 24.0 },
        { ACTION_MOUSE_SCROLL, SCROLL_DOWN, 34.0 },
        { ACTION_DELAY,        0,           GAP  }  // 组间停顿
    };

    // 自动计算指令总数，供 DLL 遍历
    int stepCount = sizeof(mySteps) / sizeof(MacroStep);

    // [高级玩家自定义区] - 纯手写 C++ 逻辑
    // 玩家可以在这里定义自己的工具函数
    void MySecretLogic() {
        // 比如：只有在按下左键+空格时，才额外按一下 Q
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            // 这里可以直接写任何 WinAPI
        }
    }

    // 核心钩子：main 循环每一帧都会调用这里
    void Custom() {
        MySecretLogic();

        // 甚至可以实时修改 mySteps 里的数据
        // mySteps[0].duration = 50.0; 
    }
}

// ======================== 执行引擎框架 ========================
int main() {

    system("chcp 65001");
    // 设置 DLL 搜索路径（为了美观，DLL 放在 ./dll 文件夹下）
    SetDllDirectoryA("./dll");

    // 初始化 Windows 高精度时钟
    timeBeginPeriod(1);

    // 封装配置包喂给 DLL
    MacroConfig config;
    config.triggerKey = UserMacro::TRIGGER_KEY;
    config.steps = UserMacro::mySteps;
    config.stepCount = UserMacro::stepCount;

    bool is_running = true;

    // 控制台 UI
    std::system("cls");
    std::cout << "========================================" << std::endl;
    std::cout << ">>> DearMacro 全能指令引擎已就绪" << std::endl;
    std::cout << ">>> 触发按键: [SPACE] | 退出程序: [END]" << std::endl;
    std::cout << ">>> 指令总数: " << UserMacro::stepCount << " 步" << std::endl;
    std::cout << "========================================" << std::endl;

    // 主监控循环
    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        // 调用 DLL 导出的核心解析函数
        // DLL 内部会处理：检测触发键 -> 遍历指令数组 -> 发送硬件信号 -> 精确休眠
        RunMacroRuntime(&config, &is_running);

        UserMacro::Custom();    
        // 预防按键未触发时的 CPU 空转
        Sleep(1);
    }

    // 清理资源
    timeEndPeriod(1);
    std::cout << ">>> 引擎已安全退出。" << std::endl;
    return 0;

}
