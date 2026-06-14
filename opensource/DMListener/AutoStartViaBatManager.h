#pragma once
#include <string>
#include <filesystem>

class AutoStartViaBatManager {
public:
    explicit AutoStartViaBatManager(const std::string& appName);
    bool IsEnabled() const;
    void SetEnabled(bool enable);
    std::filesystem::path GetBatFilePath() const;

private:
    std::string m_appName;
    std::filesystem::path GetStartupFolderPath() const;
    std::string GetExePath() const;
};
