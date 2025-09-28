#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <chrono>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <regex>
#include <mutex>

using namespace std;
mutex coutMutex; 

// RTSS 提供的核心功能头文件
// Powered By AsulTop , suki
#include "RTSSApi/rtss-core.h"

// CS2 Console Watcher 项目头文件
//#include "ReadConsole/RegistryReader.h"
//#include "ReadConsole/VdfParser.h"
//#include "ReadConsole/LogWatcher.h"

#define YAML_CPP_STATIC_DEFINE


namespace RTSSController {

    vector<thread> threads;

    static bool ReadConsole = false; 
    static atomic<bool> running{ true };

    struct KeyBind {
        int vk = 0;
        int fps = 0;
        bool hold = false;
        bool active = false;
        bool delay = true;
        chrono::steady_clock::time_point pressTime;
    };


    static map<string, vector<KeyBind>> profileBinds;
    static map<string, int> keyMap = {
        {"SPACE", VK_SPACE}, {"ESCAPE", VK_ESCAPE}, {"F1", VK_F1}, {"F2", VK_F2},
        {"F3", VK_F3}, {"F4", VK_F4}, {"F5", VK_F5}, {"F6", VK_F6},
        {"A", 'A'}, {"B", 'B'}, {"C", 'C'}, {"D", 'D'}, {"E", 'E'}, {"Q", 'Q'}
    };

    int GetKeyVCode(const string& keyName) {
        auto it = keyMap.find(keyName);
        return (it != keyMap.end()) ? it->second : 0;
    }

    void CreateDefaultConfig(const filesystem::path& configPath) {
        if (!filesystem::exists(configPath.parent_path())) {
            filesystem::create_directories(configPath.parent_path());
        }

        ofstream fout(configPath);
        if (!fout.is_open()) {
            cerr << "[ERROR] 无法创建配置文件: " << configPath << endl;
            return;
        }

        fout << "ReadConsole: false\n";
        fout << "cs2.exe:\n";
        fout << "  binds:\n";
        fout << "    - key: SPACE\n";
        fout << "      fps: 64\n";
        fout << "      hold: true\n";
        fout << "      delay: true\n";
        fout.close();

        cout << "[INFO] 未找到配置，已生成默认配置到: " << configPath << endl;
    }


    void LoadConfig(const string& configPath) {
        try {
            YAML::Node config = YAML::LoadFile(configPath);

            if (config["ReadConsole"]) {
                ReadConsole = config["ReadConsole"].as<bool>();
            }

            for (auto it = config.begin(); it != config.end(); ++it) {
                string profileName = it->first.as<string>();
                if (profileName == "ReadConsole") continue;

                YAML::Node profileNode = it->second;
                if (profileNode["binds"]) {
                    for (const auto& bindNode : profileNode["binds"]) {
                        KeyBind b;
                        b.vk = GetKeyVCode(bindNode["key"].as<string>());
                        b.fps = bindNode["fps"].as<int>();
                        b.hold = bindNode["hold"].as<bool>();
                        if (bindNode["delay"]) {
                            b.delay = bindNode["delay"].as<bool>();
                        }
                        if (b.vk != 0) profileBinds[profileName].push_back(b);
                    }
                }

            }

            cout << "[INFO] 配置已加载，profile 数量: "
                << profileBinds.size() << endl;
        }
        catch (const YAML::Exception& e) {
            cerr << "[ERROR] 加载配置失败: " << e.what() << endl;
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
                const string& profileName = profileEntry.first;
                auto& binds = profileEntry.second;

                for (auto& b : binds) {
                    if (b.hold) {
                        if (GetAsyncKeyState(b.vk) & 0x8000) {
                            if (!b.active) {
                                if (b.delay) {
                                    // 延迟模式
                                    if (b.pressTime.time_since_epoch().count() == 0) {
                                        b.pressTime = chrono::steady_clock::now();
                                    }

                                    auto now = chrono::steady_clock::now();
                                    auto held = chrono::duration_cast<chrono::milliseconds>(now - b.pressTime).count();

                                    if (held >= 1250) {
                                        SetProperty(profileName, "FramerateLimit", b.fps);
                                        b.active = true;

                                        lock_guard<mutex> lock(coutMutex);
                                        cout << "[DEBUG] " << profileName
                                            << " 按住键触发(>1.25s): 锁帧 " << b.fps << " FPS"
                                            << endl;
                                    }
                                }
                                else {
                                    // 立即触发模式
                                    SetProperty(profileName, "FramerateLimit", b.fps);
                                    b.active = true;

                                    lock_guard<mutex> lock(coutMutex);
                                    cout << "[DEBUG] " << profileName
                                        << " 按住键触发(立即): 锁帧 " << b.fps << " FPS"
                                        << endl;
                                }
                            }
                        }
                        else {
                            if (b.active) {
                                SetProperty(profileName, "FramerateLimit", 0);
                                b.active = false;

                                lock_guard<mutex> lock(coutMutex);
                                cout << "[DEBUG] " << profileName
                                    << " 松开键: 解锁帧率"
                                    << endl;
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
                                lock_guard<mutex> lock(coutMutex);
                                cout << "[DEBUG] " << profileName
                                    << " 按键触发: "
                                    << (b.active ? "锁帧 " + to_string(b.fps) + " FPS" : "解锁帧率")
                                    << endl;
                            }

                            this_thread::sleep_for(chrono::milliseconds(250));
                        }
                    }
                }
            }
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }



    void RunThreaded() {
        filesystem::path configPath = filesystem::current_path() / "rtss_config.yml";

        if (!filesystem::exists(configPath)) {
            CreateDefaultConfig(configPath);
        }

        LoadConfig(configPath.string());

        int totalBinds = CountAllBinds();
        if (totalBinds == 0 && !ReadConsole) return;

        threads.emplace_back(KeyListener);

        if (ReadConsole) {
            cerr << "不支持读控制台，请使用DearMoments" << endl;
        }

        while (running) {
            this_thread::sleep_for(chrono::milliseconds(100));
        }

        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }
    }


} // namespace RTSSController



// -------------------- main --------------------
int main() {
    thread mainThread(RTSSController::RunThreaded);
    mainThread.join();  
    return 0;
}

