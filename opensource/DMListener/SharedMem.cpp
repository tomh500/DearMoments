#include "SharedMem.h"

#include <fstream>
#include <string>
#include <windows.h>
#include <cstring>   // strcpy_s
#include <iterator>
#include "Tools.h"
#include "Global.h"

// ========== 内部工具函数 ==========
static bool ReadVersionFromFile(
    const std::filesystem::path& versionPath,
    std::string& outVersion
) {
    std::ifstream file(versionPath, std::ios::binary);
    if (!file.is_open())
        return false;

    std::string content(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    // 去 UTF-8 BOM
    if (content.size() >= 3 &&
        (uint8_t)content[0] == 0xEF &&
        (uint8_t)content[1] == 0xBB &&
        (uint8_t)content[2] == 0xBF) {
        content.erase(0, 3);
    }

    // 去尾部换行
    while (!content.empty() &&
        (content.back() == '\n' || content.back() == '\r' || content.back() == ' '))
        content.pop_back();

    if (content.empty())
        return false;

    outVersion = content;
    return true;
}

// ========== 内部状态 ==========
static HANDLE hMapFile = NULL;
static SharedInfo* pSharedData = nullptr;


int g_LocalVersion=localVersion;

namespace SharedMem {
    
    void DefVersion()
    {
    if (pSharedData && pSharedData->version[0]) {
        g_LocalVersion = atoi(pSharedData->version);
    }
    else {
        g_LocalVersion = localVersion;
    }
    }

}

// ========== 对外接口 ==========
void PublishVersionFromFile(const std::filesystem::path& RootPath)
{
    std::filesystem::path versionPath =
        RootPath / L"library" / L"resource" / L"version.txt";

    std::string versionStr;
    if (!ReadVersionFromFile(versionPath, versionStr)) {
        versionStr = "unknown";
    }

    hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SharedInfo),
        "KICore_Win32_Ver"
    );

    if (!hMapFile)
        return;

    pSharedData = (SharedInfo*)MapViewOfFile(
        hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        sizeof(SharedInfo)
    );

    if (!pSharedData) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
        return;
    }

    memset(pSharedData, 0, sizeof(SharedInfo));

    // 防止溢出
    if (versionStr.size() >= sizeof(pSharedData->version))
        versionStr.resize(sizeof(pSharedData->version) - 1);

    strcpy_s(
        pSharedData->version,
        sizeof(pSharedData->version),
        versionStr.c_str()
    );

    strcpy_s(
        pSharedData->module_name,
        sizeof(pSharedData->module_name),
        "Win32_UI_Core"
    );

    pSharedData->is_active = 1;
}

void CleanupSharedMemory()
{
    if (pSharedData) {
        pSharedData->is_active = 0;
        UnmapViewOfFile(pSharedData);
        pSharedData = nullptr;
    }
    if (hMapFile) {
        CloseHandle(hMapFile);
        hMapFile = NULL;
    }

}

bool TryReadSharedVersion(const char* shmName,std::wstring& outVersion) 
{
    HANDLE hMap = OpenFileMappingA(FILE_MAP_READ, FALSE, shmName);
    if (!hMap)
        return false;

    auto* info = (SharedInfo*)MapViewOfFile(
        hMap, FILE_MAP_READ, 0, 0, sizeof(SharedInfo)
    );

    if (!info) {
        CloseHandle(hMap);
        return false;
    }

    bool ok = info->is_active != 0;

    if (ok) {
        outVersion = Utf8ToWide(info->version);
    }

    UnmapViewOfFile(info);
    CloseHandle(hMap);
    return ok;
}



//文件位置共享内存

#pragma pack(push, 1)
struct SharedConfig {
    char profile_path[MAX_PATH]; // 固定长度，存路径字符串
    bool is_ready;               // 标志位
};
#pragma pack(pop)
// 1. 声明为全局变量，保证程序不退，句柄不灭
HANDLE g_hMapFile_Path = NULL;
SharedConfig* g_pSharedConfig = nullptr;
void ShareSettingPath() {
    // 1. 拼接路径: RootPath / "setting" / "config.yml"
    // 注意：建议直接指向具体的 yml 文件，否则 B 程序不知道读哪个
    std::filesystem::path targetPath = RootPath / L"setting" / L"QuickstopConfig.yml";

    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedConfig), "KICore_Path_Share");

    if (hMapFile) {
        SharedConfig* pBuf = (SharedConfig*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedConfig));
        if (pBuf) {
            // 2. 转换并拷贝
            // string() 在 Windows 下会根据系统区域设置转码，u8string() 则强制 UTF-8
            std::string pathStr = targetPath.string();

            strncpy_s(pBuf->profile_path, pathStr.c_str(), _TRUNCATE);
            pBuf->is_ready = true;

            std::cout << "[SHM] 路径已广播: " << pBuf->profile_path << std::endl;

            // 这里千万不要 UnmapViewOfFile 和 CloseHandle
            // 否则这块内存就消失了，B 程序啥也读不到
        }
    }
}

