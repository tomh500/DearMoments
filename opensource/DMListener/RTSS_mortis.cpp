
#define YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC
#define YAML_CPP_API
#include <yaml-cpp/yaml.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>
#include <filesystem>
#include <regex>
#include <mutex>
std::mutex coutMutex; 

// RTSS 提供的核心功能头文件
#include "RTSSApi/rtss-core.h"

// CS2 Console Watcher 项目头文件
#include "ReadConsole/RegistryReader.h"
#include "ReadConsole/VdfParser.h"
#include "ReadConsole/LogWatcher.h"






namespace RTSSController {

    std::vector<std::thread> threads;
    std::mutex coutMutex;
    std::atomic<bool> running{ true };
    bool ReadConsole = false;

    struct KeyBind {
        int vk = 0;
        int fps = 0;
        bool hold = false;
        bool active = false;
        bool delay = true;
        std::chrono::steady_clock::time_point pressTime; 
    };

    std::map<std::string, std::vector<KeyBind>> profileBinds;

    std::map<std::string, int> keyMap = {
        // 字母和数字
        {"0", '0'}, {"1", '1'}, {"2", '2'}, {"3", '3'}, {"4", '4'},
        {"5", '5'}, {"6", '6'}, {"7", '7'}, {"8", '8'}, {"9", '9'},
        {"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'},
        {"F", 'F'}, {"G", 'G'}, {"H", 'H'}, {"I", 'I'}, {"J", 'J'},
        {"K", 'K'}, {"L", 'L'}, {"M", 'M'}, {"N", 'N'}, {"O", 'O'},
        {"P", 'P'}, {"Q", 'Q'}, {"R", 'R'}, {"S", 'S'}, {"T", 'T'},
        {"U", 'U'}, {"V", 'V'}, {"W", 'W'}, {"X", 'X'}, {"Y", 'Y'},
        {"Z", 'Z'},

        // 功能键和特殊键
        {"SPACE", VK_SPACE}, {"ESCAPE", VK_ESCAPE}, {"ENTER", VK_RETURN},
        {"TAB", VK_TAB}, {"BACKSPACE", VK_BACK}, {"SHIFT", VK_SHIFT},
        {"CTRL", VK_CONTROL}, {"ALT", VK_MENU}, {"CAPSLOCK", VK_CAPITAL},
        {"PRINTSCREEN", VK_SNAPSHOT}, {"SCROLLLOCK", VK_SCROLL}, {"PAUSE", VK_PAUSE},
        {"INSERT", VK_INSERT}, {"DELETE", VK_DELETE}, {"HOME", VK_HOME},
        {"END", VK_END}, {"PAGEUP", VK_PRIOR}, {"PAGEDOWN", VK_NEXT},
        {"LEFT", VK_LEFT}, {"UP", VK_UP}, {"RIGHT", VK_RIGHT}, {"DOWN", VK_DOWN},

        // F 功能键
        {"F1", VK_F1}, {"F2", VK_F2}, {"F3", VK_F3}, {"F4", VK_F4},
        {"F5", VK_F5}, {"F6", VK_F6}, {"F7", VK_F7}, {"F8", VK_F8},
        {"F9", VK_F9}, {"F10", VK_F10}, {"F11", VK_F11}, {"F12", VK_F12},

        // 标点符号和特殊字符
        // 注意：某些虚拟键码可能因键盘布局而异。
        {";", VK_OEM_1},     // 通常是 ; 和 :
        {"=", VK_OEM_PLUS},  // 通常是 = 和 +
        {",", VK_OEM_COMMA}, // 通常是 , 和 <
        {"-", VK_OEM_MINUS}, // 通常是 - 和 _
        {".", VK_OEM_PERIOD},// 通常是 . 和 >
        {"/", VK_OEM_2},     // 通常是 / 和 ?
        {"`", VK_OEM_3},     // 通常是 ` 和 ~
        {"[", VK_OEM_4},     // 通常是 [ 和 {
        {"\\", VK_OEM_5},    // 通常是 \ 和 |
        {"]", VK_OEM_6},     // 通常是 ] 和 }
        {"'", VK_OEM_7},     // 通常是 ' 和 "

        // 小键盘
        {"NUMPAD0", VK_NUMPAD0}, {"NUMPAD1", VK_NUMPAD1}, {"NUMPAD2", VK_NUMPAD2},
        {"NUMPAD3", VK_NUMPAD3}, {"NUMPAD4", VK_NUMPAD4}, {"NUMPAD5", VK_NUMPAD5},
        {"NUMPAD6", VK_NUMPAD6}, {"NUMPAD7", VK_NUMPAD7}, {"NUMPAD8", VK_NUMPAD8},
        {"NUMPAD9", VK_NUMPAD9},
        {"MULTIPLY", VK_MULTIPLY}, {"ADD", VK_ADD}, {"SEPARATOR", VK_SEPARATOR},
        {"SUBTRACT", VK_SUBTRACT}, {"DECIMAL", VK_DECIMAL}, {"DIVIDE", VK_DIVIDE},
        {"NUMLOCK", VK_NUMLOCK},
    };

    int GetKeyVCode(const std::string& keyName) {
        auto it = keyMap.find(keyName);
        return (it != keyMap.end()) ? it->second : 0;
    }

    void CreateDefaultConfig(const std::filesystem::path& configPath) {
        // 确保目录存在
        if (!std::filesystem::exists(configPath.parent_path())) {
            std::filesystem::create_directories(configPath.parent_path());
        }

        std::ofstream fout(configPath);
        fout << "ReadConsole: false\n";
        fout << "cs2.exe:\n";
        fout << "  binds:\n";
        fout << "    - key: SPACE\n";
        fout << "      fps: 64\n";
        fout << "      hold: true\n";
        fout << "      delay: true\n";
        fout.close();
        std::wcout << L"[INFO] 未找到配置，已生成默认配置到: " << configPath.wstring() << std::flush<< std::endl;
    }

    void LoadConfig(const std::filesystem::path& configPath) {
        try {
            YAML::Node config = YAML::LoadFile(configPath.string());

            if (config["ReadConsole"]) {
                ReadConsole = config["ReadConsole"].as<bool>();
            }

            for (auto it = config.begin(); it != config.end(); ++it) {
                std::string profileName = it->first.as<std::string>();
                if (profileName == "ReadConsole") continue;

                YAML::Node profileNode = it->second;
                if (profileNode["binds"]) {
                    for (const auto& bindNode : profileNode["binds"]) {
                        KeyBind b;
                        b.vk = GetKeyVCode(bindNode["key"].as<std::string>());
                        b.fps = bindNode["fps"].as<int>();
                        b.hold = bindNode["hold"].as<bool>();
                        b.delay = bindNode["delay"].as<bool>();
                        if (b.vk != 0) profileBinds[profileName].push_back(b);
                    }
                }
            }

            std::wcout << L"[INFO] 配置已加载，profile 数量: "
                << profileBinds.size() << std::flush<<std::endl;
        }
        catch (const YAML::Exception& e) {
            std::cerr << "[ERROR] 加载配置失败: " << e.what() << std::flush<< std::endl;
        }
    }

    int CountAllBinds() {
        int total = 0;
        for (auto& entry : profileBinds) total += static_cast<int>(entry.second.size());
        return total;
    }

    void KeyListener() {
        while (running) {
            for (auto& profileEntry : profileBinds) {
                const std::string& profileName = profileEntry.first;
                auto& binds = profileEntry.second;

                for (auto& b : binds) {
                    if (b.hold) {
                        if (GetAsyncKeyState(b.vk) & 0x8000) {
                            if (!b.active) {
                                if (b.delay) {
                                    // 延迟模式：超过1.25s才触发
                                    if (b.pressTime.time_since_epoch().count() == 0) {
                                        b.pressTime = std::chrono::steady_clock::now();
                                    }

                                    auto now = std::chrono::steady_clock::now();
                                    auto held = std::chrono::duration_cast<std::chrono::milliseconds>(now - b.pressTime).count();

                                    if (held >= 800) {
                                        SetProperty(profileName, "FramerateLimit", b.fps);
                                        b.active = true;

                                        std::lock_guard<std::mutex> lock(coutMutex);
                                        std::cout << "[DEBUG] " << profileName
                                            << " 按住键触发(>1.25s): 锁帧 " << b.fps << " FPS"
                                            << std::flush<< std::endl;
                                    }
                                }
                                else {
                                    // 立即触发模式
                                    SetProperty(profileName, "FramerateLimit", b.fps);
                                    b.active = true;

                                    std::lock_guard<std::mutex> lock(coutMutex);
                                    std::cout << "[DEBUG] " << profileName
                                        << " 按住键触发(立即): 锁帧 " << b.fps << " FPS"
                                        << std::flush<<std::endl;
                                }
                            }
                        }
                        else {
                            // 松开键时
                            if (b.active) {
                                SetProperty(profileName, "FramerateLimit", 0);
                                b.active = false;

                                std::lock_guard<std::mutex> lock(coutMutex);
                                std::cout << "[DEBUG] " << profileName
                                    << " 松开键: 解锁帧率"
                                    << std::flush<< std::endl;
                            }
                            b.pressTime = {}; // 重置计时器
                        }
                    }

                    else {
                        if (GetAsyncKeyState(b.vk) & 0x0001) {
                            SetProperty(profileName, "FramerateLimit", b.active ? 0 : b.fps);
                            b.active = !b.active;

                            // 打印切换信息
                            {
                                std::lock_guard<std::mutex> lock(coutMutex);
                                std::cout << "[DEBUG] " << profileName
                                    << " 按键触发: "
                                    << (b.active ? "锁帧 " + std::to_string(b.fps) + " FPS" : "解锁帧率")
                                    << std::flush<<std::endl;
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void ConsoleWatcherThread() {
        using namespace CS2ConsoleWatcher;
        try {
            std::wstring steamPath = RegistryReader::GetSteamPath();
            std::wstring logPath = VdfParser::FindCSLogPath(steamPath);

            std::ifstream file;
            std::streampos lastSize = 0;
            bool firstRun = true;
            std::regex logPrefix(R"(^\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2}\s+)");

            while (RTSSController::running) {
                if (std::filesystem::exists(logPath)) {
                    auto fileSize = std::filesystem::file_size(logPath);

                    if (fileSize < lastSize) lastSize = 0;

                    if (fileSize > lastSize) {
                        file.open(logPath, std::ios::binary);
                        if (file) {
                            file.seekg(lastSize);

                            std::string line;
                            while (std::getline(file, line)) {
                                if (!line.empty() &&
                                    (unsigned char)line[0] == 0xEF &&
                                    line.size() >= 3 &&
                                    (unsigned char)line[1] == 0xBB &&
                                    (unsigned char)line[2] == 0xBF) {
                                    line = line.substr(3);
                                }

                                std::string rawLine = std::regex_replace(line, logPrefix, "");

                                if (!firstRun) {
                                    std::cout << rawLine << std::endl;

                                    const std::string cmd = "/fps_set ";
                                    auto pos = rawLine.find(cmd);
                                    if (pos != std::string::npos) {
                                        try {
                                            std::string numStr = rawLine.substr(pos + cmd.length());
                                            size_t end = numStr.find(";");
                                            if (end != std::string::npos) numStr = numStr.substr(0, end);
                                            int fps = std::stoi(numStr);
                                            SetProperty("cs2.exe", "FramerateLimit", fps);
                                            std::cout << "[INFO] 控制台命令生效: /fps_set " << fps << std::flush<< std::endl;
                                        }
                                        catch (...) {}
                                    }
                                }
                            }

                            lastSize = file.tellg();
                            if (lastSize == -1) lastSize = fileSize;
                            file.close();
                        }
                    }
                }

                firstRun = false;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
        catch (const std::exception& ex) {
            std::cerr << "[ERROR] 控制台监视失败: " << ex.what() << std::flush<<std::endl;
        }
    }

    void RunThreaded() {
        // 获取程序自身路径
        wchar_t path_buffer[MAX_PATH];
        GetModuleFileNameW(NULL, path_buffer, MAX_PATH);
        std::filesystem::path programPath(path_buffer);

        // 向上追溯两层目录
        std::filesystem::path rootPath = programPath.parent_path().parent_path().parent_path();
        std::filesystem::path configPath = rootPath / L"setting" / L"RTSS_mortis.yml";

        if (!std::filesystem::exists(configPath)) {
            CreateDefaultConfig(configPath);
        }
        LoadConfig(configPath);

        int totalBinds = CountAllBinds();
        if (totalBinds == 0 && !ReadConsole) return;

        threads.emplace_back(KeyListener);

        if (ReadConsole) threads.emplace_back(ConsoleWatcherThread);

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
} // namespace RTSSController