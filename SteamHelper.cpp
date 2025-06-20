#include "SteamHelper.h"
#include <iostream>
#include <string>
#include <Windows.h>
#include <filesystem>
#include <unordered_set> // 使用 unordered_set
#include <codecvt>
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

namespace fs = std::filesystem;

// 构造函数：初始化 Steam 路径并加载 userdata 文件夹
SteamHelper::SteamHelper() {
    SteamPath = CallRegister2Steam();
    if (SteamPath != L"Read Failed") {
        LoadSteamUserIDs();
    }
}

// 读取注册表获取 Steam 安装路径
std::wstring SteamHelper::CallRegister2Steam() {
    HKEY hKey;
    const std::wstring keyPath = L"SOFTWARE\\Valve\\Steam";
    const std::wstring valueName = L"SteamPath";  // Steam 安装路径存储的键名

    // 打开注册表键
    if (RegOpenKeyEx(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return L"Read Failed";
    }

    wchar_t buffer[512];
    DWORD bufferSize = sizeof(buffer);

    // 读取键的值
    if (RegQueryValueEx(hKey, valueName.c_str(), nullptr, nullptr, (LPBYTE)buffer, &bufferSize) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return L"Read Failed";
    }

    RegCloseKey(hKey);

    return std::wstring(buffer);
}

// 加载 Steam 用户ID（即 userdata 文件夹中的所有文件夹）
void SteamHelper::LoadSteamUserIDs() {
    if (SteamPath.empty()) return;

    fs::path userdataPath = fs::path(SteamPath) / L"userdata";

    if (!fs::exists(userdataPath) || !fs::is_directory(userdataPath)) {
        std::wcout << L"Invalid userdata folder." << std::endl;
        return;
    }

    // 遍历 userdata 文件夹，获取所有文件夹的名字
    for (const auto& entry : fs::directory_iterator(userdataPath)) {
        if (fs::is_directory(entry)) {
            // 转换为 string 存储 SteamID
            std::wstring steamID_w = entry.path().filename().wstring();
            std::string steamID(steamID_w.begin(), steamID_w.end());
            SteamUserIDs.push_back(steamID);
        }
    }
}

// 验证给定的 SteamID 是否存在于列表中
bool SteamHelper::VerSteamID(const std::string& GsiSteamID) const {
    for (const auto& id : SteamUserIDs) {
        if (id == GsiSteamID) {
            return true;  // 匹配成功
        }
    }
    return false;  // 匹配失败
}

// 获取 GsiSteamID
const std::string& SteamHelper::GetGsiSteamID() const {
    return GsiSteamID;
}

// 设置 GsiSteamID
void SteamHelper::SetGsiSteamID(const std::string& GsiSteamID_) {
    GsiSteamID = GsiSteamID_;
}

// 获取 Steam 用户 ID 列表
const std::vector<std::string>& SteamHelper::GetSteamUserIDs() const {
    return SteamUserIDs;
}

// 将 32 位 SteamID 转换为 64 位
std::string SteamHelper::ConvertToSteam64ID(const std::string& steam32ID) const {
    unsigned long long base = 76561197960265728;
    unsigned long long steam32 = std::stoull(steam32ID);  // 将 32 位 ID 字符串转换为数字
    unsigned long long steam64 = steam32 + base;
    return std::to_string(steam64);  // 转换为字符串并返回
}

// 将 32 位 Steam ID 列表转换为 64 位 Steam ID 集合
std::unordered_set<std::string> SteamHelper::ConvertAllToSteam64IDs() const {
    std::unordered_set<std::string> steam64IDs;
    for (const auto& steam32ID : SteamUserIDs) {
        steam64IDs.insert(ConvertToSteam64ID(steam32ID));
    }
    return steam64IDs;
}
