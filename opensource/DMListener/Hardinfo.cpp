#include <intrin.h>
#include "framework.h"
#include <wincrypt.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "advapi32.lib")

// ===================== 读取注册表字符串 =====================
static std::string ReadRegString(
    HKEY root,
    const char* path,
    const char* name
) {
    HKEY hKey;
    char buf[256] = {};
    DWORD size = sizeof(buf);

    if (RegOpenKeyExA(root, path, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return "";

    if (RegQueryValueExA(hKey, name, nullptr, nullptr, (LPBYTE)buf, &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return "";
    }

    RegCloseKey(hKey);
    return std::string(buf);
}

// ===================== MachineGuid（系统级唯一） =====================
static std::string GetMachineGuid()
{
    return ReadRegString(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Cryptography",
        "MachineGuid"
    );
}

// ===================== SMBIOS UUID（主板级唯一） =====================
static std::string GetSystemUUID()
{
    return ReadRegString(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\SystemInformation",
        "SystemUUID"
    );
}

// ===================== 物理内存容量（辅助熵） =====================
static std::string GetPhysicalMemory()
{
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);

    std::ostringstream oss;
    oss << std::hex << ms.ullTotalPhys;
    return oss.str();
}

// ===================== SHA1 =====================
static std::string Sha1(const std::string& input)
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[20];
    DWORD len = 20;

    CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
    CryptHashData(hHash, (BYTE*)input.data(), (DWORD)input.size(), 0);
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &len, 0);

    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream oss;
    for (DWORD i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

// ===================== 最终【机器唯一码】 =====================
std::string GetHardInfo()
{
    std::string raw =
        "GUID=" + GetMachineGuid() + "|" +
        "UUID=" + GetSystemUUID() + "|" +
        "MEM=" + GetPhysicalMemory();

    return Sha1(raw);
}

std::string g_LocalHardInfo = GetHardInfo();
