#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <sstream>
#include <filesystem>
#include <vector>
#include <regex>
#include <any>
#include <typeinfo>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include "Constants.h"
#include "Logger.h"

namespace Core {

enum class ConfigFormat {
    CFG,
    YAML,
    JSON,
    AUTO_DETECT
};

class ConfigManager {
private:
    mutable std::mutex configMutex;
    std::unordered_map<std::string, std::any> configData;
    std::vector<std::string> loadedFiles;
    
    ConfigManager() {
        // Load existing configuration files on initialization
        loadFallbackConfigs();
    }
    
    void loadFallbackConfigs() {
        // Load existing CFG files
        loadConfig(Constants::ConfigFiles::USER_SETTING, ConfigFormat::CFG);
        loadConfig(Constants::ConfigFiles::USER_KEYBINDS, ConfigFormat::CFG);
        loadConfig(Constants::ConfigFiles::USER_VALUE, ConfigFormat::CFG);
        
        // Load YAML file
        loadConfig(Constants::ConfigFiles::RTSS_MORTIS, ConfigFormat::YAML);
        
        // Load JSON file if exists
        if (std::filesystem::exists(Constants::ConfigFiles::GSI_CONFIG)) {
            loadConfig(Constants::ConfigFiles::GSI_CONFIG, ConfigFormat::JSON);
        }
        
        LOG_INFO_STREAM("ConfigManager initialized with " << loadedFiles.size() << " configuration files");
    }
    
    ConfigFormat detectFormat(const std::string& filePath) const {
        std::string extension = std::filesystem::path(filePath).extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        
        if (extension == ".cfg") return ConfigFormat::CFG;
        if (extension == ".yml" || extension == ".yaml") return ConfigFormat::YAML;
        if (extension == ".json") return ConfigFormat::JSON;
        
        return ConfigFormat::CFG; // Default fallback
    }
    
    bool parseCfgFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_WARNING_STREAM("Could not open CFG file: " << filePath);
            return false;
        }
        
        std::string line;
        int lineNumber = 0;
        
        while (std::getline(file, line)) {
            lineNumber++;
            
            // Skip empty lines and comments
            line = trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#' || line.substr(0, 2) == "//") {
                continue;
            }
            
            // Parse key=value pairs
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string key = trim(line.substr(0, equalPos));
                std::string value = trim(line.substr(equalPos + 1));
                
                // Remove quotes if present - safe version with proper empty check
                if (!value.empty() && 
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.length() - 2);
                }
                
                configData[key] = value;
            }
        }
        
        file.close();
        LOG_DEBUG_STREAM("Loaded CFG file: " << filePath << " (" << lineNumber << " lines processed)");
        return true;
    }
    
    bool parseYamlFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_WARNING_STREAM("Could not open YAML file: " << filePath);
            return false;
        }
        
        std::string line;
        int lineNumber = 0;
        
        while (std::getline(file, line)) {
            lineNumber++;
            
            // Skip empty lines and comments
            line = trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            // Simple YAML parsing (key: value)
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = trim(line.substr(0, colonPos));
                std::string value = trim(line.substr(colonPos + 1));
                
                // Remove quotes if present - safe version with proper empty check
                if (!value.empty() && 
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.length() - 2);
                }
                
                configData[key] = value;
            }
        }
        
        file.close();
        LOG_DEBUG_STREAM("Loaded YAML file: " << filePath << " (" << lineNumber << " lines processed)");
        return true;
    }
    
    bool parseJsonFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_WARNING_STREAM("Could not open JSON file: " << filePath);
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();
        
        // Simple JSON parsing (basic key-value pairs)
        // This is a simplified parser - for production use a proper JSON library
        std::regex jsonPattern(R"("([^"]+)"\s*:\s*"([^"]*)")");
        std::sregex_iterator iter(content.begin(), content.end(), jsonPattern);
        std::sregex_iterator end;
        
        int parsedPairs = 0;
        for (; iter != end; ++iter) {
            std::smatch match = *iter;
            std::string key = match[1].str();
            std::string value = match[2].str();
            configData[key] = value;
            parsedPairs++;
        }
        
        LOG_DEBUG_STREAM("Loaded JSON file: " << filePath << " (" << parsedPairs << " key-value pairs)");
        return parsedPairs > 0;
    }
    
    std::string trim(const std::string& str) const {
        size_t first = str.find_first_not_of(' ');
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(' ');
        return str.substr(first, (last - first + 1));
    }
    
    std::string generateBackupPath(const std::string& originalPath) const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << originalPath << ".backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        return ss.str();
    }

public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }
    
    bool loadConfig(const std::string& filePath, ConfigFormat format = ConfigFormat::AUTO_DETECT) {
        std::lock_guard<std::mutex> lock(configMutex);
        
        if (!std::filesystem::exists(filePath)) {
            LOG_WARNING_STREAM("Configuration file does not exist: " << filePath);
            return false;
        }
        
        if (format == ConfigFormat::AUTO_DETECT) {
            format = detectFormat(filePath);
        }
        
        bool success = false;
        switch (format) {
            case ConfigFormat::CFG:
                success = parseCfgFile(filePath);
                break;
            case ConfigFormat::YAML:
                success = parseYamlFile(filePath);
                break;
            case ConfigFormat::JSON:
                success = parseJsonFile(filePath);
                break;
            default:
                LOG_ERROR_STREAM("Unsupported configuration format for file: " << filePath);
                return false;
        }
        
        if (success) {
            loadedFiles.push_back(filePath);
            LOG_INFO_STREAM("Successfully loaded configuration: " << filePath);
        } else {
            LOG_ERROR_STREAM("Failed to load configuration: " << filePath);
        }
        
        return success;
    }
    
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue) const {
        std::lock_guard<std::mutex> lock(configMutex);
        
        auto it = configData.find(key);
        if (it == configData.end()) {
            LOG_DEBUG_STREAM("Configuration key not found, using default: " << key);
            return defaultValue;
        }
        
        try {
            // Handle string values
            if constexpr (std::is_same_v<T, std::string>) {
                return std::any_cast<std::string>(it->second);
            }
            // Handle integer values
            else if constexpr (std::is_same_v<T, int>) {
                std::string strValue = std::any_cast<std::string>(it->second);
                return std::stoi(strValue);
            }
            // Handle float values
            else if constexpr (std::is_same_v<T, float>) {
                std::string strValue = std::any_cast<std::string>(it->second);
                return std::stof(strValue);
            }
            // Handle boolean values
            else if constexpr (std::is_same_v<T, bool>) {
                std::string strValue = std::any_cast<std::string>(it->second);
                std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
                return strValue == "true" || strValue == "1" || strValue == "yes" || strValue == "on";
            }
            else {
                LOG_ERROR_STREAM("Unsupported type for configuration key: " << key);
                return defaultValue;
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR_STREAM("Error converting configuration value for key '" << key << "': " << e.what());
            return defaultValue;
        }
    }
    
    template<typename T>
    void setValue(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(configMutex);
        
        if constexpr (std::is_same_v<T, std::string>) {
            configData[key] = value;
        } else {
            configData[key] = std::to_string(value);
        }
        
        LOG_DEBUG_STREAM("Configuration value set: " << key << " = " << value);
    }
    
    bool saveConfig(const std::string& filePath, ConfigFormat format = ConfigFormat::CFG) {
        std::lock_guard<std::mutex> lock(configMutex);
        
        // Create backup if file exists
        if (std::filesystem::exists(filePath)) {
            std::string backupPath = generateBackupPath(filePath);
            try {
                std::filesystem::copy_file(filePath, backupPath);
                LOG_INFO_STREAM("Configuration backup created: " << backupPath);
            }
            catch (const std::exception& e) {
                LOG_WARNING_STREAM("Could not create backup: " << e.what());
            }
        }
        
        std::ofstream file(filePath);
        if (!file.is_open()) {
            LOG_ERROR_STREAM("Could not open file for writing: " << filePath);
            return false;
        }
        
        // Write header comment (not for JSON)
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        if (format != ConfigFormat::JSON) {
            file << "# Configuration file generated by Summer Controller" << std::endl;
            file << "# Generated: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
            file << std::endl;
        }
        
        // Write configuration data
        if (format == ConfigFormat::JSON) {
            file << "{" << std::endl;
            bool first = true;
            for (const auto& [key, value] : configData) {
                try {
                    std::string strValue = std::any_cast<std::string>(value);
                    if (!first) {
                        file << "," << std::endl;
                    }
                    file << "  \"" << key << "\": \"" << strValue << "\"";
                    first = false;
                }
                catch (const std::exception& e) {
                    LOG_WARNING_STREAM("Could not write configuration key '" << key << "': " << e.what());
                }
            }
            file << std::endl << "}" << std::endl;
        } else {
            for (const auto& [key, value] : configData) {
                try {
                    std::string strValue = std::any_cast<std::string>(value);
                    
                    switch (format) {
                        case ConfigFormat::CFG:
                            file << key << "=" << strValue << std::endl;
                            break;
                        case ConfigFormat::YAML:
                            file << key << ": " << strValue << std::endl;
                            break;
                        default:
                            file << key << "=" << strValue << std::endl;
                            break;
                    }
                }
                catch (const std::exception& e) {
                    LOG_WARNING_STREAM("Could not write configuration key '" << key << "': " << e.what());
                }
            }
        }
        
        file.close();
        LOG_INFO_STREAM("Configuration saved to: " << filePath);
        return true;
    }
    
    std::vector<std::string> getLoadedFiles() const {
        std::lock_guard<std::mutex> lock(configMutex);
        return loadedFiles;
    }
    
    size_t getConfigCount() const {
        std::lock_guard<std::mutex> lock(configMutex);
        return configData.size();
    }
    
    bool hasKey(const std::string& key) const {
        std::lock_guard<std::mutex> lock(configMutex);
        return configData.find(key) != configData.end();
    }
    
    void clearConfig() {
        std::lock_guard<std::mutex> lock(configMutex);
        configData.clear();
        loadedFiles.clear();
        LOG_INFO("Configuration data cleared");
    }
    
    void reloadConfigs() {
        std::lock_guard<std::mutex> lock(configMutex);
        std::vector<std::string> filesToReload = loadedFiles;
        configData.clear();
        loadedFiles.clear();
        
        for (const auto& filePath : filesToReload) {
            loadConfig(filePath);
        }
        
        LOG_INFO_STREAM("Reloaded " << filesToReload.size() << " configuration files");
    }
};


} // namespace Core

// Convenience macros for configuration access
#define GET_CONFIG(key, defaultValue) Core::ConfigManager::getInstance().getValue(key, defaultValue)
#define SET_CONFIG(key, value) Core::ConfigManager::getInstance().setValue(key, value)
#define HAS_CONFIG(key) Core::ConfigManager::getInstance().hasKey(key)

#endif // CONFIG_MANAGER_H
