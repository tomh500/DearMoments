#pragma once
#ifndef STEAMHELPER_H
#define STEAMHELPER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>

// 宏定义：用于 DLL 的导出与导入
#ifdef STEAMHELPER_EXPORTS
#define STEAM_API __declspec(dllexport)
#else
#define STEAM_API __declspec(dllimport)
#endif

namespace fs = std::filesystem;

class STEAM_API SteamHelper {
public:
    /**
     * @brief 构造函数
     * 逻辑：自动调用 CallRegister2Steam 获取路径，如果获取成功，紧接着调用 LoadSteamUserIDs 加载本地账号
     */
    SteamHelper();
    ~SteamHelper();

    /**
     * @brief 从 Windows 注册表读取 Steam 安装目录
     * @return std::wstring 返回 Steam 的绝对路径（如 L"C:/Program Files (x86)/Steam"）
     * @note 它是 static 静态函数，意味着你不需要实例化类就能通过 SteamHelper::CallRegister2Steam() 调用它
     */
    static std::wstring CallRegister2Steam();

    /**
     * @brief 扫描 Steam 目录下的 userdata 文件夹
     * 逻辑：遍历 userdata 下的所有子文件夹，每一个数字命名的文件夹其实就是一个 Steam32 ID
     */
    void LoadSteamUserIDs();

    /**
     * @brief 校验某个 SteamID 是否在本地登录过
     * @param GsiSteamID 需要验证的 32 位 ID 字符串
     * @return bool 存在返回 true，否则返回 false
     */
    bool VerSteamID(const std::string& GsiSteamID) const;

    /**
     * @brief 获取当前类实例中保存的 GsiSteamID
     */
    const std::string& GetGsiSteamID() const;

    /**
     * @brief 设置类实例中的 GsiSteamID
     * @param GsiSteamID_ 传入的 ID 字符串
     */
    void SetGsiSteamID(const std::string& GsiSteamID_);

    /**
     * @brief 获取本地扫描到的所有 Steam32 ID 列表
     * @return const std::vector<std::string>& 返回一个只读引用的列表
     */
    const std::vector<std::string>& GetSteamUserIDs() const;

    /**
     * @brief 单个转换：将 32 位 ID 转换为 64 位 ID
     * @param steam32ID 类似 "12345678"
     * @return std::string 类似 "76561198083722406"
     */
    std::string ConvertToSteam64ID(const std::string& steam32ID) const;

    /**
     * @brief 批量转换：将本地扫描到的所有 32 位 ID 转换并存入集合
     * @return std::unordered_set<std::string> 返回一个不重复的 64 位 ID 集合
     */
    std::unordered_set<std::string> ConvertAllToSteam64IDs() const;

private:
    std::wstring SteamPath;                  // 缓存 Steam 的安装路径
    std::vector<std::string> SteamUserIDs;   // 缓存扫描到的 32 位用户 ID 列表
    std::string GsiSteamID = "114514";       // 内部操作用的临时 ID 变量
};

#endif