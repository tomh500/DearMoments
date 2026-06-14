#pragma once
#define YAML_CPP_STATIC_DEFINE
#define YAML_CPP_STATIC
#define YAML_CPP_API       
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

// RTSS 提供的核心功能头文件
#include "RTSSApi/rtss-core.h"

// CS2 Console Watcher 项目头文件
#include "ReadConsole/RegistryReader.h"
#include "ReadConsole/VdfParser.h"
#include "ReadConsole/LogWatcher.h"


namespace RTSSController {

    extern std::vector<std::thread> threads;
    extern std::mutex coutMutex;
    extern std::atomic<bool> running;
    extern bool ReadConsole;

    struct KeyBind {
        int vk;
        int fps;
        bool hold;
        bool active;
    };

    extern std::map<std::string, std::vector<KeyBind>> profileBinds;
    extern std::map<std::string, int> keyMap;

    int GetKeyVCode(const std::string& keyName);

    void CreateDefaultConfig(const std::string& configPath);
    void LoadConfig(const std::string& configPath);
    int CountAllBinds();

    void KeyListener();
    void ConsoleWatcherThread();
    void RunThreaded();

} // namespace RTSSController
