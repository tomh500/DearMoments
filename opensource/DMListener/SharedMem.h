#pragma once
#include <filesystem>
#include <cstdint>

// 共享结构（所有模块必须一致）
#pragma pack(push, 1)
struct SharedInfo {
    char version[32];
    char module_name[32];
    uint8_t is_active;
};
#pragma pack(pop)

// 对外接口
void PublishVersionFromFile(const std::filesystem::path& RootPath);
void CleanupSharedMemory();
bool TryReadSharedVersion(const char* shmName, std::wstring& outVersion);
void ShareSettingPath();

extern int g_LocalVersion;

namespace SharedMem {

    void DefVersion();
}