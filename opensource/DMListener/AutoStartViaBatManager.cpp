#include "AutoStartViaBatManager.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>

AutoStartViaBatManager::AutoStartViaBatManager(const std::string& appName)
    : m_appName(appName) {
}

std::string AutoStartViaBatManager::GetExePath() const {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return std::string(path);
}

std::filesystem::path AutoStartViaBatManager::GetBatFilePath() const {
    PWSTR startupPath = nullptr;
    std::filesystem::path result;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &startupPath))) {
        result = std::filesystem::path(startupPath) / L"Summer.bat";
        CoTaskMemFree(startupPath);
    }
    return result;
}

std::filesystem::path AutoStartViaBatManager::GetStartupFolderPath() const {
    PWSTR path = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, NULL, &path))) {
        result = path;  // wchar_t* 自动构造 std::filesystem::path
        CoTaskMemFree(path);
    }
    return result;
}

bool AutoStartViaBatManager::IsEnabled() const {
    auto batPath = GetBatFilePath();
    return std::filesystem::exists(batPath);
}

void AutoStartViaBatManager::SetEnabled(bool enable) {
    auto batPath = GetBatFilePath();
    if (batPath.empty()) return;

    auto exePath = GetExePath();
    auto exeDir = std::filesystem::path(exePath).parent_path();

    if (enable) {
        std::ofstream bat(batPath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!bat) return;

        bat << "@echo off\n";
        bat << "pushd \"" << exeDir.string() << "\"\n";
        bat << "start \"\" \"" << exePath << "\"\n";
        bat << "popd\n";
    }
    else {
        std::error_code ec;
        std::filesystem::remove(batPath, ec);
    }
}
