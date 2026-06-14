#include <windows.h>
#include <filesystem>
#include <fstream>

bool ExtractResource(unsigned int resId,
    const std::filesystem::path& outFile)
{
    HMODULE hMod = GetModuleHandleW(nullptr);
    // 1. 找资源
    HRSRC hRes = FindResourceW(hMod,
        MAKEINTRESOURCEW(resId),
        RT_RCDATA);
    if (!hRes) return false;

    // 2. 加载资源到内存
    HGLOBAL hData = LoadResource(hMod, hRes);
    if (!hData) return false;

    // 3. 取资源大小和指针
    DWORD dataSize = SizeofResource(hMod, hRes);
    void* pData = LockResource(hData);
    if (!pData || dataSize == 0) return false;

    // 4. 确保目标目录存在
    std::filesystem::create_directories(outFile.parent_path());

    // 5. 写入文件
    std::ofstream ofs(outFile, std::ios::binary);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(pData), dataSize);
    return ofs.good();
}
