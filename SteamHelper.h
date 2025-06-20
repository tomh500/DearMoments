#pragma once
#ifndef STEAMHELPER_H
#define STEAMHELPER_H

#include <string>
#include <vector>
#include <unordered_set> // 使用 unordered_set 来存储 64 位 Steam ID
#include <filesystem>  // 必须包含文件系统头文件

namespace fs = std::filesystem;  // 定义 fs 为 std::filesystem 的别名

class SteamHelper {
public:
    // 构造函数：初始化 Steam 路径并加载 userdata 文件夹
    SteamHelper();

    // 读取注册表获取 Steam 安装路径
    static std::wstring CallRegister2Steam();

    // 加载 Steam 用户ID（即 userdata 文件夹中的所有文件夹）
    void LoadSteamUserIDs();

    // 验证给定的 SteamID 是否存在于列表中
    bool VerSteamID(const std::string& GsiSteamID) const;

    // 获取 GsiSteamID
    const std::string& GetGsiSteamID() const;

    // 设置 GsiSteamID
    void SetGsiSteamID(const std::string& GsiSteamID_);

    // 获取 Steam 用户 ID 列表
    const std::vector<std::string>& GetSteamUserIDs() const;

    // 将 32 位 SteamID 转换为 64 位
    std::string ConvertToSteam64ID(const std::string& steam32ID) const;

    // 将 32 位 Steam ID 列表转换为 64 位 Steam ID 集合
    std::unordered_set<std::string> ConvertAllToSteam64IDs() const;

private:
    std::wstring SteamPath;                  // Steam 安装路径
    std::vector<std::string> SteamUserIDs;   // 用户ID列表
    std::string GsiSteamID = "114514";       // 默认的 SteamID
};

#endif // STEAMHELPER_H
