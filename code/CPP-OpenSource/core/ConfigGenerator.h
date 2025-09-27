#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <mutex>
#include <memory>
#include "ModuleLoader.h"
#include "EventSystem.h"
#include "Logger.h"
#include "SafeOperation.h"
#include "Constants.h"

namespace Core {

/**
 * Configuration generation result containing generated content and metadata
 */
struct ConfigGenerationResult {
    bool success;
    std::string content;
    std::string filePath;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::chrono::system_clock::time_point generatedAt;
    size_t moduleCount;
    
    ConfigGenerationResult() : success(false), generatedAt(std::chrono::system_clock::now()), moduleCount(0) {}
};

/**
 * Template system for configuration generation with parameter substitution
 */
class ConfigTemplate {
private:
    std::string m_template;
    std::unordered_map<std::string, std::string> m_variables;
    
public:
    explicit ConfigTemplate(const std::string& templateStr) : m_template(templateStr) {}
    
    void setVariable(const std::string& name, const std::string& value) {
        m_variables[name] = value;
    }
    
    std::string generate() const {
        std::string result = m_template;
        for (const auto& pair : m_variables) {
            std::string placeholder = "${" + pair.first + "}";
            size_t pos = 0;
            while ((pos = result.find(placeholder, pos)) != std::string::npos) {
                result.replace(pos, placeholder.length(), pair.second);
                pos += pair.second.length();
            }
        }
        return result;
    }
    
    void clearVariables() { m_variables.clear(); }
    size_t getVariableCount() const { return m_variables.size(); }
};

/**
 * Comprehensive configuration generator that automates _init_.cfg file creation
 * Integrates with ModuleLoader for module discovery and EventSystem for notifications
 */
class ConfigGenerator {
private:
    static ConfigGenerator* s_instance;
    static std::mutex s_instanceMutex;
    
    std::string m_autumnBasePath;
    std::vector<std::string> m_generatedFiles;
    mutable std::mutex m_generationMutex;
    
    // Event system integration
    SubscriptionId m_moduleLoadedSubscription;
    SubscriptionId m_moduleUnloadedSubscription;
    
    // Configuration templates
    std::unordered_map<std::string, ConfigTemplate> m_templates;
    
    // Private constructor for singleton
    ConfigGenerator();

public:
    // Singleton access
    static ConfigGenerator& getInstance();
    
    // Destructor
    ~ConfigGenerator();
    
    // Non-copyable
    ConfigGenerator(const ConfigGenerator&) = delete;
    ConfigGenerator& operator=(const ConfigGenerator&) = delete;

    // Configuration Generation Pipeline
    Safe::Result<ConfigGenerationResult> generateAllConfigurations();
    Safe::Result<ConfigGenerationResult> generateMainConfig();
    Safe::Result<ConfigGenerationResult> generateToolsConfig();
    Safe::Result<void> regenerateIfNeeded();
    
    // Individual Configuration Generation
    Safe::Result<std::string> generateModuleConfig(const std::string& moduleName);
    Safe::Result<std::string> generateExecStatements(const std::vector<std::string>& moduleNames);
    
    // Configuration Validation
    Safe::Result<void> validateConfiguration(const std::string& configPath);
    Safe::Result<void> validateCS2Syntax(const std::string& content);
    
    // Backup and Recovery
    Safe::Result<void> createBackup(const std::string& configPath);
    Safe::Result<void> restoreFromBackup(const std::string& configPath);
    Safe::Result<std::vector<std::string>> listBackups(const std::string& configPath);
    
    // Template Management
    void addTemplate(const std::string& name, const std::string& templateContent);
    Safe::Result<std::string> renderTemplate(const std::string& templateName, 
                                            const std::unordered_map<std::string, std::string>& variables);
    
    // File Management
    Safe::Result<void> writeConfigFile(const std::string& filePath, const std::string& content);
    Safe::Result<std::string> readConfigFile(const std::string& filePath);
    bool configFileExists(const std::string& filePath);
    
    // Event Handlers
    void onModuleLoaded(const ModuleLoadedEvent& event);
    void onModuleUnloaded(const ModuleUnloadedEvent& event);
    
    // Statistics and Information
    std::vector<std::string> getGeneratedFiles() const;
    size_t getGeneratedFileCount() const;
    
    // Cleanup
    void cleanup();

private:
    // Initialization
    void initializeTemplates();
    void subscribeToEvents();
    void unsubscribeFromEvents();
    
    // Path Management
    std::string getModulePath() const;
    std::string getToolsPath() const;
    std::string getBackupPath(const std::string& originalPath) const;
    std::string generateTimestampedBackupPath(const std::string& originalPath) const;
    
    // Content Generation
    std::string generateHeader(const std::string& configType, size_t moduleCount) const;
    std::string generateFooter() const;
    std::string generateModuleExecSection(const std::vector<std::string>& moduleNames) const;
    
    // Module Processing
    std::vector<std::string> getOrderedModuleNames() const;
    std::string getModuleDisplayName(const std::string& moduleName) const;
    std::string getModuleConfigPath(const std::string& moduleName) const;
    
    // File Operations
    Safe::Result<void> ensureDirectoryExists(const std::string& path);
    Safe::Result<void> atomicWriteFile(const std::string& filePath, const std::string& content);
    
    // Validation Helpers
    bool isValidCS2Command(const std::string& line) const;
    std::vector<std::string> findSyntaxErrors(const std::string& content) const;
    
    // Backup Management
    void cleanupOldBackups(const std::string& configPath, size_t maxBackups = 3);
};

// Singleton Implementation
inline ConfigGenerator* ConfigGenerator::s_instance = nullptr;
inline std::mutex ConfigGenerator::s_instanceMutex;

inline ConfigGenerator::ConfigGenerator() 
    : m_autumnBasePath(Constants::Paths::AUTUMN_BASE_PATH), m_moduleLoadedSubscription(0), m_moduleUnloadedSubscription(0) {
    initializeTemplates();
    subscribeToEvents();
    LOG_INFO("ConfigGenerator instance created");
}

inline ConfigGenerator::~ConfigGenerator() {
    cleanup();
    LOG_INFO("ConfigGenerator instance destroyed");
}

inline ConfigGenerator& ConfigGenerator::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = new ConfigGenerator();
    }
    return *s_instance;
}

inline Safe::Result<ConfigGenerationResult> ConfigGenerator::generateAllConfigurations() {
    std::lock_guard<std::mutex> lock(m_generationMutex);
    
    try {
        LOG_INFO("Starting comprehensive configuration generation");
        
        ConfigGenerationResult result;
        result.generatedAt = std::chrono::system_clock::now();
        
        // Publish start event
        ConfigGenerationStartedEvent startEvent;
        EventSystem::getInstance().publish(startEvent);
        
        // Generate main module configuration
        auto mainResult = generateMainConfig();
        if (mainResult.isSuccess()) {
            auto mainConfig = mainResult.getValue();
            result.moduleCount = mainConfig.moduleCount;
            result.content += mainConfig.content;
            m_generatedFiles.push_back(mainConfig.filePath);
        } else {
            result.errors.push_back("Failed to generate main config: " + mainResult.getError().message);
        }
        
        // Generate tools configuration
        auto toolsResult = generateToolsConfig();
        if (toolsResult.isSuccess()) {
            auto toolsConfig = toolsResult.getValue();
            m_generatedFiles.push_back(toolsConfig.filePath);
        } else {
            result.warnings.push_back("Failed to generate tools config: " + toolsResult.getError().message);
        }
        
        // Determine overall success
        result.success = result.errors.empty();
        
        if (result.success) {
            LOG_INFO_STREAM("Configuration generation completed successfully. Generated " << result.moduleCount << " module configurations");
            
            ConfigGenerationCompletedEvent completedEvent(result.moduleCount, m_generatedFiles);
            EventSystem::getInstance().publish(completedEvent);
        } else {
            LOG_ERROR_STREAM("Configuration generation completed with errors: " << result.errors.size());
            
            ConfigGenerationFailedEvent failedEvent(result.errors);
            EventSystem::getInstance().publish(failedEvent);
        }
        
        return Safe::Result<ConfigGenerationResult>::success(result);
        
    } catch (const std::exception& e) {
        ConfigGenerationResult errorResult;
        errorResult.errors.push_back("Exception during generation: " + std::string(e.what()));
        
        ConfigGenerationFailedEvent failedEvent(errorResult.errors);
        EventSystem::getInstance().publish(failedEvent);
        
        return Safe::Result<ConfigGenerationResult>::failure(
            Safe::SafeError("GENERATION_EXCEPTION", "Exception during configuration generation: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<ConfigGenerationResult> ConfigGenerator::generateMainConfig() {
    try {
        ConfigGenerationResult result;
        std::string configPath = getModulePath() + "/_init_.cfg";
        
        LOG_INFO_STREAM("Generating main configuration: " << configPath);
        
        // Create backup of existing file
        if (configFileExists(configPath)) {
            auto backupResult = createBackup(configPath);
            if (!backupResult.isSuccess()) {
                result.warnings.push_back("Failed to create backup: " + backupResult.getError().message);
            }
        }
        
        // Get ordered module names from ModuleLoader
        auto moduleNames = getOrderedModuleNames();
        result.moduleCount = moduleNames.size();
        
        // Generate configuration content
        std::ostringstream content;
        
        // Add header
        content << generateHeader("Main Module Configuration", moduleNames.size()) << "\n";
        
        // Add tools initialization first
        content << "exec \"" << getToolsPath() << "/_init_.cfg\"\n\n";
        
        // Add module exec statements
        content << generateModuleExecSection(moduleNames) << "\n";
        
        // Add footer
        content << generateFooter();
        
        result.content = content.str();
        result.filePath = configPath;
        
        // Validate generated content
        auto validateResult = validateCS2Syntax(result.content);
        if (!validateResult.isSuccess()) {
            result.warnings.push_back("Syntax validation warning: " + validateResult.getError().message);
        }
        
        // Write configuration file
        auto writeResult = writeConfigFile(configPath, result.content);
        if (writeResult.isSuccess()) {
            result.success = true;
            LOG_INFO_STREAM("Successfully generated main configuration with " << moduleNames.size() << " modules");
        } else {
            result.success = false;
            result.errors.push_back("Failed to write config file: " + writeResult.getError().message);
        }
        
        return Safe::Result<ConfigGenerationResult>::success(result);
        
    } catch (const std::exception& e) {
        return Safe::Result<ConfigGenerationResult>::failure(
            Safe::SafeError("MAIN_CONFIG_GENERATION_FAILED", "Failed to generate main config: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<ConfigGenerationResult> ConfigGenerator::generateToolsConfig() {
    try {
        ConfigGenerationResult result;
        std::string configPath = getToolsPath() + "/_init_.cfg";
        
        LOG_INFO_STREAM("Generating tools configuration: " << configPath);
        
        // Create backup of existing file
        if (configFileExists(configPath)) {
            auto backupResult = createBackup(configPath);
            if (!backupResult.isSuccess()) {
                result.warnings.push_back("Failed to create backup: " + backupResult.getError().message);
            }
        }
        
        // Generate tools configuration content
        std::ostringstream content;
        
        // Add header
        content << generateHeader("Tools Configuration", 0) << "\n";
        
        // Add tool-specific configurations
        content << "// Tool configurations\n";
        content << "exec \"" << Constants::Paths::TOOLS_DIR << "/CFGLoader/_init_.cfg\"\n";
        content << "exec \"" << Constants::Paths::TOOLS_DIR << "/Output/_init_.cfg\"\n\n";
        
        // Add footer
        content << generateFooter();
        
        result.content = content.str();
        result.filePath = configPath;
        result.moduleCount = 2; // CFGLoader and Output
        
        // Write configuration file
        auto writeResult = writeConfigFile(configPath, result.content);
        if (writeResult.isSuccess()) {
            result.success = true;
            LOG_INFO_STREAM("Successfully generated tools configuration");
        } else {
            result.success = false;
            result.errors.push_back("Failed to write tools config: " + writeResult.getError().message);
        }
        
        return Safe::Result<ConfigGenerationResult>::success(result);
        
    } catch (const std::exception& e) {
        return Safe::Result<ConfigGenerationResult>::failure(
            Safe::SafeError("TOOLS_CONFIG_GENERATION_FAILED", "Failed to generate tools config: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<void> ConfigGenerator::validateCS2Syntax(const std::string& content) {
    auto errors = findSyntaxErrors(content);
    if (!errors.empty()) {
        std::string errorMsg = "CS2 syntax errors found: ";
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i > 0) errorMsg += ", ";
            errorMsg += errors[i];
        }
        return Safe::Result<void>::failure(Safe::SafeError("SYNTAX_ERROR", errorMsg));
    }
    return Safe::Result<void>::success();
}

inline Safe::Result<void> ConfigGenerator::createBackup(const std::string& configPath) {
    try {
        if (!std::filesystem::exists(configPath)) {
            return Safe::Result<void>::success(); // No file to backup
        }
        
        std::string backupPath = generateTimestampedBackupPath(configPath);
        
        // Create backup directory if needed
        auto dirResult = ensureDirectoryExists(std::filesystem::path(backupPath).parent_path().string());
        if (!dirResult.isSuccess()) {
            return dirResult;
        }
        
        // Copy file to backup location
        std::filesystem::copy_file(configPath, backupPath, std::filesystem::copy_options::overwrite_existing);
        
        LOG_INFO_STREAM("Created backup: " << backupPath);
        
        // Cleanup old backups
        cleanupOldBackups(configPath);
        
        return Safe::Result<void>::success();
        
    } catch (const std::filesystem::filesystem_error& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("BACKUP_FAILED", "Filesystem error creating backup: " + std::string(e.what()))
        );
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("BACKUP_EXCEPTION", "Exception creating backup: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<void> ConfigGenerator::writeConfigFile(const std::string& filePath, const std::string& content) {
    auto result = atomicWriteFile(filePath, content);
    
    // Publish ConfigChangedEvent after successful write
    if (result.isSuccess()) {
        Core::ConfigChangedEvent configEvent(filePath, "updated");
        Core::EventSystem::getInstance().publish(configEvent);
    }
    
    return result;
}

inline bool ConfigGenerator::configFileExists(const std::string& filePath) {
    return std::filesystem::exists(filePath);
}

// Event Handlers
inline void ConfigGenerator::onModuleLoaded(const ModuleLoadedEvent& event) {
    LOG_INFO_STREAM("Module loaded event received: " << event.getModuleName() << ". Triggering configuration regeneration.");
    
    auto result = regenerateIfNeeded();
    if (!result.isSuccess()) {
        LOG_ERROR_STREAM("Failed to regenerate configuration after module load: " << result.getError().message);
    }
}

inline void ConfigGenerator::onModuleUnloaded(const ModuleUnloadedEvent& event) {
    LOG_INFO_STREAM("Module unloaded event received: " << event.getModuleName() << ". Triggering configuration regeneration.");
    
    auto result = regenerateIfNeeded();
    if (!result.isSuccess()) {
        LOG_ERROR_STREAM("Failed to regenerate configuration after module unload: " << result.getError().message);
    }
}

// Private Implementation

inline void ConfigGenerator::initializeTemplates() {
    // Main config template
    std::string mainTemplate = R"(
// ${CONFIG_TYPE}
// Generated by Autumn ConfigGenerator
// Timestamp: ${TIMESTAMP}
// Module count: ${MODULE_COUNT}
// DO NOT EDIT MANUALLY - This file is auto-generated

${CONTENT}

// End of auto-generated configuration
)";
    m_templates.emplace("main", ConfigTemplate(mainTemplate));
}

inline void ConfigGenerator::subscribeToEvents() {
    auto& eventSystem = EventSystem::getInstance();
    
    m_moduleLoadedSubscription = eventSystem.subscribe<ModuleLoadedEvent>(
        [this](const ModuleLoadedEvent& event) { onModuleLoaded(event); }
    );
    
    m_moduleUnloadedSubscription = eventSystem.subscribe<ModuleUnloadedEvent>(
        [this](const ModuleUnloadedEvent& event) { onModuleUnloaded(event); }
    );
}

inline void ConfigGenerator::unsubscribeFromEvents() {
    auto& eventSystem = EventSystem::getInstance();
    eventSystem.unsubscribe<ModuleLoadedEvent>(m_moduleLoadedSubscription);
    eventSystem.unsubscribe<ModuleUnloadedEvent>(m_moduleUnloadedSubscription);
}

inline std::string ConfigGenerator::getModulePath() const {
    return Constants::Paths::MODULES_DIR;
}

inline std::string ConfigGenerator::getToolsPath() const {
    return Constants::Paths::TOOLS_DIR;
}

inline std::string ConfigGenerator::generateTimestampedBackupPath(const std::string& originalPath) const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    std::filesystem::path path(originalPath);
    std::string backupDir = path.parent_path().string() + "/.backups";
    std::string filename = path.stem().string() + "_" + std::to_string(timestamp) + ".bak";
    
    return backupDir + "/" + filename;
}

inline std::string ConfigGenerator::generateHeader(const std::string& configType, size_t moduleCount) const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    
    std::ostringstream header;
    header << "// " << configType << "\n";
    header << "// Generated by Autumn ConfigGenerator\n";
    header << "// Timestamp: " << timestamp << "\n";
    header << "// Module count: " << moduleCount << "\n";
    header << "// DO NOT EDIT MANUALLY - This file is auto-generated\n";
    
    return header.str();
}

inline std::string ConfigGenerator::generateFooter() const {
    return "\n// End of auto-generated configuration\n";
}

inline std::string ConfigGenerator::generateModuleExecSection(const std::vector<std::string>& moduleNames) const {
    std::ostringstream section;
    section << "// Module execution section\n";
    
    for (const auto& moduleName : moduleNames) {
        std::string configPath = getModuleConfigPath(moduleName);
        section << "exec \"" << configPath << "\"\n";
    }
    
    return section.str();
}

inline std::vector<std::string> ConfigGenerator::getOrderedModuleNames() const {
    auto moduleNames = ModuleLoader::getInstance().getLoadedModules();
    
    // Sort by priority (ascending) then by name (ascending) for deterministic order
    std::sort(moduleNames.begin(), moduleNames.end(), [](const std::string& a, const std::string& b) {
        auto& loader = ModuleLoader::getInstance();
        auto moduleA = loader.getModule(a);
        auto moduleB = loader.getModule(b);
        
        if (moduleA.isSuccess() && moduleB.isSuccess()) {
            const auto& infoA = moduleA.getValue()->getModuleInfo();
            const auto& infoB = moduleB.getValue()->getModuleInfo();
            
            // First sort by priority (lower priority loads first)
            if (infoA.priority != infoB.priority) {
                return infoA.priority < infoB.priority;
            }
            
            // Then sort by name for consistent ordering
            return infoA.name < infoB.name;
        }
        
        // Fallback to name comparison if module info unavailable
        return a < b;
    });
    
    return moduleNames;
}

inline std::string ConfigGenerator::getModuleConfigPath(const std::string& moduleName) const {
    // Fetch ModuleInfo via ModuleLoader::getInstance().getModuleInfo(name) and build path with relativePath
    auto infoResult = ModuleLoader::getInstance().getModuleInfo(moduleName);
    if (infoResult.isSuccess()) {
        const auto& info = infoResult.getValue();
        return m_autumnBasePath + "/Modules/" + info.relativePath + "/_init_.cfg";
    }
    
    // Fallback to old behavior if module info unavailable
    return m_autumnBasePath + "/Modules/" + moduleName + "/_init_.cfg";
}

inline Safe::Result<void> ConfigGenerator::ensureDirectoryExists(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
        return Safe::Result<void>::success();
    } catch (const std::filesystem::filesystem_error& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("DIRECTORY_CREATION_FAILED", "Failed to create directory: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<void> ConfigGenerator::atomicWriteFile(const std::string& filePath, const std::string& content) {
    try {
        // Ensure parent directory exists
        auto dirResult = ensureDirectoryExists(std::filesystem::path(filePath).parent_path().string());
        if (!dirResult.isSuccess()) {
            return dirResult;
        }
        
        // Write to temporary file first
        std::string tempPath = filePath + ".tmp";
        {
            std::ofstream file(tempPath, std::ios::binary);
            if (!file) {
                return Safe::Result<void>::failure(
                    Safe::SafeError("FILE_OPEN_FAILED", "Failed to open temp file for writing: " + tempPath)
                );
            }
            file << content;
        }
        
        // Remove existing file if it exists (Windows compatibility)
        if (std::filesystem::exists(filePath)) {
            std::filesystem::remove(filePath);
        }
        
        // Atomically move temp file to final location
        std::filesystem::rename(tempPath, filePath);
        
        return Safe::Result<void>::success();
        
    } catch (const std::filesystem::filesystem_error& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("FILE_WRITE_FAILED", "Filesystem error writing file: " + std::string(e.what()))
        );
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("FILE_WRITE_EXCEPTION", "Exception writing file: " + std::string(e.what()))
        );
    }
}

inline std::vector<std::string> ConfigGenerator::findSyntaxErrors(const std::string& content) const {
    std::vector<std::string> errors;
    std::istringstream stream(content);
    std::string line;
    size_t lineNumber = 0;
    
    while (std::getline(stream, line)) {
        lineNumber++;
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '/' || line[0] == ';') continue;
        
        // Basic CS2 command validation
        if (!isValidCS2Command(line)) {
            errors.push_back("Line " + std::to_string(lineNumber) + ": Invalid CS2 command syntax");
        }
    }
    
    return errors;
}

inline bool ConfigGenerator::isValidCS2Command(const std::string& line) const {
    // Basic validation for common CS2 commands
    static const std::vector<std::string> validCommands = {
        "exec", "alias", "bind", "echo", "sv_cheats", "mp_", "bot_", "cl_", "fps_max"
    };
    
    for (const auto& cmd : validCommands) {
        if (line.substr(0, cmd.length()) == cmd) {
            return true;
        }
    }
    
    // Allow any command that looks like a valid identifier
    if (!line.empty() && (std::isalpha(line[0]) || line[0] == '_')) {
        return true;
    }
    
    return false;
}

inline void ConfigGenerator::cleanupOldBackups(const std::string& configPath, size_t maxBackups) {
    try {
        std::filesystem::path path(configPath);
        std::string backupDir = path.parent_path().string() + "/.backups";
        
        if (!std::filesystem::exists(backupDir)) return;
        
        std::vector<std::filesystem::directory_entry> backupFiles;
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".bak") {
                backupFiles.push_back(entry);
            }
        }
        
        if (backupFiles.size() <= maxBackups) return;
        
        // Sort by modification time (oldest first)
        std::sort(backupFiles.begin(), backupFiles.end(),
            [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
                return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
            });
        
        // Remove oldest files
        size_t filesToRemove = backupFiles.size() - maxBackups;
        for (size_t i = 0; i < filesToRemove; ++i) {
            std::filesystem::remove(backupFiles[i].path());
            LOG_DEBUG_STREAM("Removed old backup: " << backupFiles[i].path());
        }
        
    } catch (const std::exception& e) {
        LOG_WARNING_STREAM("Failed to cleanup old backups: " << e.what());
    }
}

inline std::vector<std::string> ConfigGenerator::getGeneratedFiles() const {
    std::lock_guard<std::mutex> lock(m_generationMutex);
    return m_generatedFiles;
}

inline Safe::Result<void> ConfigGenerator::regenerateIfNeeded() {
    // Simple implementation - always regenerate when called
    auto result = generateAllConfigurations();
    return result.isSuccess() ? Safe::Result<void>::success() : 
           Safe::Result<void>::failure(Safe::SafeError("REGENERATION_FAILED", "Failed to regenerate configurations"));
}

inline void ConfigGenerator::cleanup() {
    LOG_INFO("Starting ConfigGenerator cleanup");
    unsubscribeFromEvents();
    m_generatedFiles.clear();
    m_templates.clear();
    LOG_INFO("ConfigGenerator cleanup completed");
}


inline Safe::Result<void> ConfigGenerator::validateConfiguration(const std::string& configPath) {
    try {
        auto readResult = readConfigFile(configPath);
        if (!readResult.isSuccess()) {
            return Safe::Result<void>::failure(readResult.getError());
        }
        
        return validateCS2Syntax(readResult.getValue());
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("VALIDATION_EXCEPTION", "Exception validating configuration: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<void> ConfigGenerator::restoreFromBackup(const std::string& configPath) {
    try {
        auto backupsResult = listBackups(configPath);
        if (!backupsResult.isSuccess()) {
            return Safe::Result<void>::failure(backupsResult.getError());
        }
        
        auto backups = backupsResult.getValue();
        if (backups.empty()) {
            return Safe::Result<void>::failure(
                Safe::SafeError("NO_BACKUPS_FOUND", "No backups found for: " + configPath)
            );
        }
        
        // Use the most recent backup (they're sorted by timestamp)
        std::string latestBackup = backups.back();
        
        // Copy backup to original location
        std::filesystem::copy_file(latestBackup, configPath, std::filesystem::copy_options::overwrite_existing);
        
        LOG_INFO_STREAM("Restored configuration from backup: " << latestBackup);
        return Safe::Result<void>::success();
        
    } catch (const std::filesystem::filesystem_error& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("RESTORE_FAILED", "Filesystem error restoring backup: " + std::string(e.what()))
        );
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("RESTORE_EXCEPTION", "Exception restoring backup: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<std::vector<std::string>> ConfigGenerator::listBackups(const std::string& configPath) {
    try {
        std::string backupDir = getBackupPath(configPath);
        
        if (!std::filesystem::exists(backupDir)) {
            return Safe::Result<std::vector<std::string>>::success(std::vector<std::string>{});
        }
        
        std::vector<std::string> backups;
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".bak") {
                backups.push_back(entry.path().string());
            }
        }
        
        // Sort by filename (which contains timestamp)
        std::sort(backups.begin(), backups.end());
        
        return Safe::Result<std::vector<std::string>>::success(backups);
        
    } catch (const std::filesystem::filesystem_error& e) {
        return Safe::Result<std::vector<std::string>>::failure(
            Safe::SafeError("BACKUP_LIST_FAILED", "Filesystem error listing backups: " + std::string(e.what()))
        );
    } catch (const std::exception& e) {
        return Safe::Result<std::vector<std::string>>::failure(
            Safe::SafeError("BACKUP_LIST_EXCEPTION", "Exception listing backups: " + std::string(e.what()))
        );
    }
}

inline void ConfigGenerator::addTemplate(const std::string& name, const std::string& templateContent) {
    m_templates.emplace(name, ConfigTemplate(templateContent));
    LOG_DEBUG_STREAM("Added template: " << name);
}

inline Safe::Result<std::string> ConfigGenerator::renderTemplate(const std::string& templateName, 
                                                                 const std::unordered_map<std::string, std::string>& variables) {
    auto it = m_templates.find(templateName);
    if (it == m_templates.end()) {
        return Safe::Result<std::string>::failure(
            Safe::SafeError("TEMPLATE_NOT_FOUND", "Template not found: " + templateName)
        );
    }
    
    ConfigTemplate& tmpl = it->second;
    tmpl.clearVariables();
    
    for (const auto& pair : variables) {
        tmpl.setVariable(pair.first, pair.second);
    }
    
    return Safe::Result<std::string>::success(tmpl.generate());
}

inline Safe::Result<std::string> ConfigGenerator::readConfigFile(const std::string& filePath) {
    try {
        if (!std::filesystem::exists(filePath)) {
            return Safe::Result<std::string>::failure(
                Safe::SafeError("FILE_NOT_FOUND", "Configuration file not found: " + filePath)
            );
        }
        
        std::ifstream file(filePath);
        if (!file) {
            return Safe::Result<std::string>::failure(
                Safe::SafeError("FILE_OPEN_FAILED", "Failed to open configuration file: " + filePath)
            );
        }
        
        std::ostringstream content;
        content << file.rdbuf();
        
        return Safe::Result<std::string>::success(content.str());
        
    } catch (const std::exception& e) {
        return Safe::Result<std::string>::failure(
            Safe::SafeError("READ_EXCEPTION", "Exception reading configuration file: " + std::string(e.what()))
        );
    }
}

inline size_t ConfigGenerator::getGeneratedFileCount() const {
    std::lock_guard<std::mutex> lock(m_generationMutex);
    return m_generatedFiles.size();
}

inline Safe::Result<std::string> ConfigGenerator::generateModuleConfig(const std::string& moduleName) {
    try {
        auto infoResult = ModuleLoader::getInstance().getModuleInfo(moduleName);
        if (!infoResult.isSuccess()) {
            return Safe::Result<std::string>::failure(infoResult.getError());
        }
        
        const auto& info = infoResult.getValue();
        
        std::ostringstream config;
        config << "// Module: " << info.displayName << "\n";
        config << "// Version: " << info.version << "\n";
        config << "// Description: " << info.description << "\n";
        config << "// Category: " << static_cast<int>(info.category) << "\n";
        config << "// Priority: " << info.priority << "\n\n";
        
        // Add exec statement for module's _init_.cfg
        config << "exec \"" << getModuleConfigPath(moduleName) << "\"\n";
        
        return Safe::Result<std::string>::success(config.str());
        
    } catch (const std::exception& e) {
        return Safe::Result<std::string>::failure(
            Safe::SafeError("MODULE_CONFIG_GENERATION_FAILED", "Exception generating module config: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<std::string> ConfigGenerator::generateExecStatements(const std::vector<std::string>& moduleNames) {
    try {
        std::ostringstream statements;
        statements << "// Exec statements for modules\n";
        
        for (const auto& moduleName : moduleNames) {
            std::string configPath = getModuleConfigPath(moduleName);
            statements << "exec \"" << configPath << "\"\n";
        }
        
        return Safe::Result<std::string>::success(statements.str());
        
    } catch (const std::exception& e) {
        return Safe::Result<std::string>::failure(
            Safe::SafeError("EXEC_GENERATION_FAILED", "Exception generating exec statements: " + std::string(e.what()))
        );
    }
}

// Private method implementation
inline std::string ConfigGenerator::getModuleDisplayName(const std::string& moduleName) const {
    auto infoResult = ModuleLoader::getInstance().getModuleInfo(moduleName);
    if (infoResult.isSuccess()) {
        const auto& info = infoResult.getValue();
        return info.displayName.empty() ? moduleName : info.displayName;
    }
    return moduleName; // Fallback to module name
}

inline std::string ConfigGenerator::getBackupPath(const std::string& originalPath) const {
    std::filesystem::path path(originalPath);
    return path.parent_path().string() + "/.backups";
}

// Additional Event Classes for ConfigGenerator

class ConfigGenerationStartedEvent : public Event {
public:
    ConfigGenerationStartedEvent() : Event(EventMetadata("ConfigGenerator", EventCategory::CONFIG, "Generation started")) {}
    
    std::string getEventType() const override { return "ConfigGenerationStartedEvent"; }
    std::string toString() const override { return "ConfigGenerationStartedEvent: Configuration generation started"; }
};

class ConfigGenerationCompletedEvent : public Event {
private:
    size_t m_moduleCount;
    std::vector<std::string> m_generatedFiles;
    
public:
    ConfigGenerationCompletedEvent(size_t moduleCount, const std::vector<std::string>& files)
        : Event(EventMetadata("ConfigGenerator", EventCategory::CONFIG, "Generation completed")), 
          m_moduleCount(moduleCount), m_generatedFiles(files) {}
          
    size_t getModuleCount() const { return m_moduleCount; }
    const std::vector<std::string>& getGeneratedFiles() const { return m_generatedFiles; }
    
    std::string getEventType() const override { return "ConfigGenerationCompletedEvent"; }
    std::string toString() const override { 
        return "ConfigGenerationCompletedEvent: Generated " + std::to_string(m_moduleCount) + " module configurations"; 
    }
};

class ConfigGenerationFailedEvent : public Event {
private:
    std::vector<std::string> m_errors;
    
public:
    explicit ConfigGenerationFailedEvent(const std::vector<std::string>& errors)
        : Event(EventMetadata("ConfigGenerator", EventCategory::ERROR, "Generation failed")), m_errors(errors) {}
        
    const std::vector<std::string>& getErrors() const { return m_errors; }
    
    std::string getEventType() const override { return "ConfigGenerationFailedEvent"; }
    std::string toString() const override { 
        return "ConfigGenerationFailedEvent: " + std::to_string(m_errors.size()) + " errors occurred"; 
    }
};

} // namespace Core
