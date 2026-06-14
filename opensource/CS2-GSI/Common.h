#pragma once
// 屏蔽 Windows.h 里的常用宏冲突
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <ws2tcpip.h>
#include <shlwapi.h>
#include <shellapi.h>

// 第三方库
#include <SDL.h>
#include <SDL_mixer.h>
#include <nlohmann/json.hpp>
#include <simdjson.h>
#include "cpp-httplib/httplib.h"

// 常用标准库
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Shlwapi.lib")