#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <mutex>
#include <filesystem>
#include <algorithm>
#include <shared_mutex>
#include "../modules/IModule.h"
#include "Logger.h"
#include "SafeOperation.h"
#include "Constants.h"
#include "EventSystem.h"

namespace Core {

/**
 * Module state enumeration for tracking module lifecycle
 */
enum class ModuleState {
    DISCOVERED,     // Module found but not loaded
    LOADED,         // Module instantiated
    INITIALIZED,    // Module initialized
    ACTIVE,         // Module activated and running
    ERROR           // Module in error state
};

/**
 * Module registry entry containing module and its state
 */
struct ModuleEntry {
    std::unique_ptr<Modules::Core::IModule> module;
    ModuleState state;
    std::string errorMessage;
    std::chrono::system_clock::time_point lastStateChange;
    std::vector<std::pair<std::string, Core::SubscriptionId>> subscriptions;
    
    ModuleEntry() : state(ModuleState::DISCOVERED), lastStateChange(std::chrono::system_clock::now()) {}
};

/**
 * Comprehensive module loader system for auto-discovery and management of CS2 configuration modules
 * Thread-safe singleton implementation with filesystem-based discovery and dependency resolution
 */
class ModuleLoader {
private:
    static ModuleLoader* s_instance;
    static std::mutex s_instanceMutex;
    
    std::unordered_map<std::string, ModuleEntry> m_modules;
    mutable std::shared_mutex m_modulesMutex;
    std::vector<std::string> m_discoveryPaths;
    std::vector<std::string> m_excludePatterns;
    bool m_initialized;
    
    // Private constructor for singleton
    ModuleLoader();

public:
    // Singleton access
    static ModuleLoader& getInstance();
    
    // Destructor
    ~ModuleLoader();
    
    // Non-copyable
    ModuleLoader(const ModuleLoader&) = delete;
    ModuleLoader& operator=(const ModuleLoader&) = delete;

    // Module Discovery
    Safe::Result<void> discoverModules(const std::string& basePath);
    Safe::Result<void> addDiscoveryPath(const std::string& path);
    Safe::Result<void> removeDiscoveryPath(const std::string& path);
    Safe::Result<void> addExcludePattern(const std::string& pattern);
    
    // Module Loading Pipeline
    Safe::Result<void> loadAllModules();
    Safe::Result<void> loadModule(const std::string& name);
    Safe::Result<void> unloadModule(const std::string& name);
    Safe::Result<void> reloadAllModules();
    Safe::Result<void> reloadModule(const std::string& name);
    
    // Module Access
    Safe::Result<Modules::Core::IModule*> getModule(const std::string& name);
    std::vector<std::string> getLoadedModules() const;
    std::vector<std::string> getDiscoveredModules() const;
    bool isModuleLoaded(const std::string& name) const;
    ModuleState getModuleState(const std::string& name) const;
    
    // Module Information
    std::vector<Modules::Core::ModuleInfo> getModuleInfos() const;
    Safe::Result<Modules::Core::ModuleInfo> getModuleInfo(const std::string& name) const;
    
    // Module State Management
    Safe::Result<void> activateModule(const std::string& name);
    Safe::Result<void> deactivateModule(const std::string& name);
    Safe::Result<void> activateAllModules();
    Safe::Result<void> deactivateAllModules();
    
    // Error Handling and Recovery
    std::vector<std::string> getModulesInErrorState() const;
    Safe::Result<void> clearModuleError(const std::string& name);
    Safe::Result<void> recoverFromErrors();
    
    // Cleanup
    void cleanup();

private:
    // Discovery Implementation
    Safe::Result<std::vector<std::string>> scanForModules(const std::string& path) const;
    bool isValidModulePath(const std::string& path) const;
    bool shouldExcludePath(const std::string& path) const;
    
    // Module Creation
    Safe::Result<std::unique_ptr<Modules::Core::IModule>> createLegacyModule(const std::string& configPath);
    
    // Module Ordering and Dependencies
    std::vector<std::string> sortModulesByPriority(const std::vector<std::string>& moduleNames) const;
    Safe::Result<void> validateDependencies(const std::string& moduleName) const;
    bool hasCircularDependencies(const std::string& moduleName, std::unordered_set<std::string>& visited) const;
    
    // State Management
    void setModuleState(const std::string& name, ModuleState state, const std::string& errorMessage = "");
    ModuleEntry* getModuleEntry(const std::string& name);
    const ModuleEntry* getModuleEntry(const std::string& name) const;
    
    // Thread Safety Helpers
    void writeGuard(std::function<void()> operation);
    template<typename T>
    T readGuard(std::function<T()> operation) const;
};

// Singleton Implementation
inline ModuleLoader* ModuleLoader::s_instance = nullptr;
inline std::mutex ModuleLoader::s_instanceMutex;

inline ModuleLoader::ModuleLoader() : m_initialized(false) {
    m_excludePatterns = {
        "**/.*",           // Hidden files and directories
        "**/*.bak",        // Backup files
        "**/*.tmp",        // Temporary files
        "**/Output/**",    // Output directories
        "**/bin/**",       // Binary directories
        "**/obj/**"        // Object directories
    };
    LOG_INFO("ModuleLoader instance created");
}

inline ModuleLoader::~ModuleLoader() {
    cleanup();
    LOG_INFO("ModuleLoader instance destroyed");
}

inline ModuleLoader& ModuleLoader::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = new ModuleLoader();
    }
    return *s_instance;
}

inline Safe::Result<void> ModuleLoader::discoverModules(const std::string& basePath) {
    try {
        LOG_INFO_STREAM("Starting module discovery in: " << basePath);
        
        if (!std::filesystem::exists(basePath)) {
            return Safe::Result<void>::failure(
                Safe::SafeError("PATH_NOT_FOUND", "Discovery path does not exist: " + basePath)
            );
        }
        
        auto scanResult = scanForModules(basePath);
        if (!scanResult.isSuccess()) {
            return Safe::Result<void>::failure(scanResult.getError());
        }
        
        auto modulePaths = scanResult.getValue();
        LOG_INFO_STREAM("Discovered " << modulePaths.size() << " potential modules");
        
        writeGuard([this, &modulePaths]() {
            for (const auto& path : modulePaths) {
                try {
                    auto moduleResult = createLegacyModule(path);
                    if (moduleResult.isSuccess()) {
                        auto module = std::move(moduleResult.getValue());
                        const auto& info = module->getModuleInfo();
                        
                        if (m_modules.find(info.name) == m_modules.end()) {
                            ModuleEntry entry;
                            entry.module = std::move(module);
                            entry.state = ModuleState::DISCOVERED;
                            entry.lastStateChange = std::chrono::system_clock::now();
                            
                            m_modules[info.name] = std::move(entry);
                            LOG_INFO_STREAM("Discovered module: " << info.name << " (" << path << ")");
                        } else {
                            LOG_WARNING_STREAM("Duplicate module name found: " << info.name << " at " << path);
                        }
                    } else {
                        LOG_ERROR_STREAM("Failed to create module from " << path << ": " << moduleResult.getError().message);
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR_STREAM("Exception during module creation from " << path << ": " << e.what());
                }
            }
        });
        
        LOG_INFO_STREAM("Module discovery completed. Found " << m_modules.size() << " modules");
        return Safe::Result<void>::success();
        
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("DISCOVERY_EXCEPTION", "Exception during module discovery: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<void> ModuleLoader::loadAllModules() {
    try {
        LOG_INFO("Starting module loading process");
        
        auto moduleNames = readGuard<std::vector<std::string>>([this]() {
            std::vector<std::string> names;
            for (const auto& pair : m_modules) {
                if (pair.second.state == ModuleState::DISCOVERED && 
                    pair.second.module && 
                    pair.second.module->isEnabled()) {
                    names.push_back(pair.first);
                }
            }
            return names;
        });
        
        // Sort modules by priority for proper loading order
        auto sortedNames = sortModulesByPriority(moduleNames);
        
        int loadedCount = 0;
        int errorCount = 0;
        
        for (const auto& name : sortedNames) {
            auto result = loadModule(name);
            if (result.isSuccess()) {
                loadedCount++;
                LOG_INFO_STREAM("Loaded module: " << name);
            } else {
                errorCount++;
                LOG_ERROR_STREAM("Failed to load module " << name << ": " << result.getError().message);
                setModuleState(name, ModuleState::ERROR, result.getError().message);
            }
        }
        
        LOG_INFO_STREAM("Module loading completed. Loaded: " << loadedCount << ", Errors: " << errorCount);
        
        if (errorCount > 0) {
            return Safe::Result<void>::failure(
                Safe::SafeError("PARTIAL_LOAD_FAILURE", 
                    "Some modules failed to load (" + std::to_string(errorCount) + " errors)")
            );
        }
        
        return Safe::Result<void>::success();
        
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("LOAD_EXCEPTION", "Exception during module loading: " + std::string(e.what()))
        );
    }
}

inline Safe::Result<void> ModuleLoader::loadModule(const std::string& name) {
    try {
        auto entry = getModuleEntry(name);
        if (!entry) {
            return Safe::Result<void>::failure(
                Safe::SafeError("MODULE_NOT_FOUND", "Module not found: " + name)
            );
        }
        
        if (!entry->module) {
            return Safe::Result<void>::failure(
                Safe::SafeError("INVALID_MODULE", "Invalid module instance: " + name)
            );
        }
        
        if (entry->state != ModuleState::DISCOVERED) {
            return Safe::Result<void>::failure(
                Safe::SafeError("INVALID_STATE", "Module not in discoverable state: " + name)
            );
        }
        
        // Validate dependencies
        auto depResult = validateDependencies(name);
        if (!depResult.isSuccess()) {
            return depResult;
        }
        
        // Initialize module
        auto initResult = entry->module->initialize();
        if (!initResult.isSuccess()) {
            setModuleState(name, ModuleState::ERROR, "Initialization failed: " + initResult.getError().message);
            
            // Publish module error event
            Core::ModuleErrorEvent errorEvent(name, "Initialization failed: " + initResult.getError().message);
            Core::EventSystem::getInstance().publish(errorEvent);
            
            return Safe::Result<void>::failure(initResult.getError());
        }
        
        setModuleState(name, ModuleState::INITIALIZED);
        
        // Activate module
        auto activateResult = entry->module->activate();
        if (!activateResult.isSuccess()) {
            setModuleState(name, ModuleState::ERROR, "Activation failed: " + activateResult.getError().message);
            
            // Publish module error event
            Core::ModuleErrorEvent errorEvent(name, "Activation failed: " + activateResult.getError().message);
            Core::EventSystem::getInstance().publish(errorEvent);
            
            return Safe::Result<void>::failure(activateResult.getError());
        }
        
        setModuleState(name, ModuleState::ACTIVE);
        
        // Subscribe to events based on module's event subscriptions
        auto eventSubscriptions = entry->module->getEventSubscriptions();
        entry->subscriptions.clear();
        for (const auto& eventName : eventSubscriptions) {
            if (auto id = subscribeByName(eventName, entry->module.get())) {
                entry->subscriptions.push_back({eventName, id});
            }
        }
        
        // Publish module loaded event
        Core::ModuleLoadedEvent loadedEvent(name, entry->module->getModuleInfo().version);
        Core::EventSystem::getInstance().publish(loadedEvent);
        
        return Safe::Result<void>::success();
        
    } catch (const std::exception& e) {
        setModuleState(name, ModuleState::ERROR, "Exception during load: " + std::string(e.what()));
        return Safe::Result<void>::failure(
            Safe::SafeError("LOAD_EXCEPTION", "Exception loading module " + name + ": " + e.what())
        );
    }
}

inline Safe::Result<void> ModuleLoader::unloadModule(const std::string& name) {
    try {
        auto entry = getModuleEntry(name);
        if (!entry || !entry->module) {
            return Safe::Result<void>::failure(
                Safe::SafeError("MODULE_NOT_FOUND", "Module not found: " + name)
            );
        }
        
        if (entry->state == ModuleState::ACTIVE) {
            auto deactivateResult = entry->module->deactivate();
            if (!deactivateResult.isSuccess()) {
                LOG_WARNING_STREAM("Failed to deactivate module " << name << ": " << deactivateResult.getError().message);
            }
        }
        
        if (entry->state != ModuleState::DISCOVERED) {
            auto cleanupResult = entry->module->cleanup();
            if (!cleanupResult.isSuccess()) {
                LOG_WARNING_STREAM("Failed to cleanup module " << name << ": " << cleanupResult.getError().message);
            }
        }
        
        // Unsubscribe from all events
        for (const auto& subscription : entry->subscriptions) {
            const std::string& eventName = subscription.first;
            Core::SubscriptionId subscriptionId = subscription.second;
            
            if (!unsubscribeByName(eventName, subscriptionId)) {
                LOG_WARNING_STREAM("Failed to unsubscribe from " << eventName << " for module " << name << " (ID: " << subscriptionId << ")");
            } else {
                LOG_DEBUG_STREAM("Successfully unsubscribed from " << eventName << " for module " << name << " (ID: " << subscriptionId << ")");
            }
        }
        entry->subscriptions.clear();
        
        setModuleState(name, ModuleState::DISCOVERED);
        
        // Publish module unloaded event
        Core::ModuleUnloadedEvent unloadedEvent(name);
        Core::EventSystem::getInstance().publish(unloadedEvent);
        
        LOG_INFO_STREAM("Unloaded module: " << name);
        return Safe::Result<void>::success();
        
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("UNLOAD_EXCEPTION", "Exception unloading module " + name + ": " + e.what())
        );
    }
}

inline Safe::Result<Modules::Core::IModule*> ModuleLoader::getModule(const std::string& name) {
    std::shared_lock<std::shared_mutex> lock(m_modulesMutex);
    auto it = m_modules.find(name);
    if (it != m_modules.end() && it->second.module) {
        return Safe::Result<Modules::Core::IModule*>::success(it->second.module.get());
    }
    return Safe::Result<Modules::Core::IModule*>::failure(
        Safe::SafeError("MODULE_NOT_FOUND", "Module not found: " + name)
    );
}

inline std::vector<std::string> ModuleLoader::getLoadedModules() const {
    return readGuard<std::vector<std::string>>([this]() {
        std::vector<std::string> loaded;
        for (const auto& pair : m_modules) {
            if (pair.second.state == ModuleState::ACTIVE) {
                loaded.push_back(pair.first);
            }
        }
        return loaded;
    });
}

inline bool ModuleLoader::isModuleLoaded(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_modulesMutex);
    auto it = m_modules.find(name);
    return it != m_modules.end() && it->second.state == ModuleState::ACTIVE;
}

inline ModuleState ModuleLoader::getModuleState(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(m_modulesMutex);
    auto it = m_modules.find(name);
    return it != m_modules.end() ? it->second.state : ModuleState::ERROR;
}

inline void ModuleLoader::cleanup() {
    LOG_INFO("Starting ModuleLoader cleanup");
    
    writeGuard([this]() {
        for (auto& pair : m_modules) {
            if (pair.second.module) {
                try {
                    if (pair.second.state == ModuleState::ACTIVE) {
                        pair.second.module->deactivate();
                    }
                    pair.second.module->cleanup();
                } catch (const std::exception& e) {
                    LOG_ERROR_STREAM("Exception during cleanup of module " << pair.first << ": " << e.what());
                }
            }
        }
        m_modules.clear();
    });
    
    LOG_INFO("ModuleLoader cleanup completed");
}

// Private Implementation Methods

inline Safe::Result<std::vector<std::string>> ModuleLoader::scanForModules(const std::string& path) const {
    std::vector<std::string> modulePaths;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().filename() == "_init_.cfg") {
                std::string configPath = entry.path().string();
                
                if (isValidModulePath(configPath) && !shouldExcludePath(configPath)) {
                    modulePaths.push_back(configPath);
                }
            }
        }
        
        return Safe::Result<std::vector<std::string>>::success(modulePaths);
        
    } catch (const std::filesystem::filesystem_error& e) {
        return Safe::Result<std::vector<std::string>>::failure(
            Safe::SafeError("FILESYSTEM_ERROR", "Filesystem error during scan: " + std::string(e.what()))
        );
    } catch (const std::exception& e) {
        return Safe::Result<std::vector<std::string>>::failure(
            Safe::SafeError("SCAN_EXCEPTION", "Exception during module scan: " + std::string(e.what()))
        );
    }
}

inline bool ModuleLoader::isValidModulePath(const std::string& path) const {
    // Basic path validation
    return !path.empty() && 
           path.find("_init_.cfg") != std::string::npos &&
           std::filesystem::exists(path);
}

inline bool ModuleLoader::shouldExcludePath(const std::string& path) const {
    // Explicitly check common patterns without glob magic
    
    // Return true if path contains .backups directory
    if (path.find("/.backups/") != std::string::npos) {
        return true;
    }
    
    // Return true if path contains Output directory
    if (path.find("/Output/") != std::string::npos) {
        return true;
    }
    
    // Return true if path ends with backup or temporary extensions
    if (path.ends_with(".bak") || path.ends_with(".tmp")) {
        return true;
    }
    
    // Additional exclusions for common directories
    if (path.find("/bin/") != std::string::npos || 
        path.find("/obj/") != std::string::npos ||
        path.find("/.git/") != std::string::npos) {
        return true;
    }
    
    return false;
}

inline Safe::Result<std::unique_ptr<Modules::Core::IModule>> ModuleLoader::createLegacyModule(const std::string& configPath) {
    try {
        auto module = std::make_unique<Modules::Core::LegacyModuleAdapter>(configPath);
        return Safe::Result<std::unique_ptr<Modules::Core::IModule>>::success(std::move(module));
    } catch (const std::exception& e) {
        return Safe::Result<std::unique_ptr<Modules::Core::IModule>>::failure(
            Safe::SafeError("MODULE_CREATION_FAILED", "Failed to create legacy module: " + std::string(e.what()))
        );
    }
}

inline std::vector<std::string> ModuleLoader::sortModulesByPriority(const std::vector<std::string>& moduleNames) const {
    std::vector<std::string> sorted = moduleNames;
    
    std::sort(sorted.begin(), sorted.end(), [this](const std::string& a, const std::string& b) {
        auto entryA = getModuleEntry(a);
        auto entryB = getModuleEntry(b);
        
        if (!entryA || !entryB || !entryA->module || !entryB->module) {
            return a < b; // Fallback to alphabetical
        }
        
        int priorityA = entryA->module->getModuleInfo().priority;
        int priorityB = entryB->module->getModuleInfo().priority;
        
        if (priorityA != priorityB) {
            return priorityA < priorityB; // Lower priority loads first
        }
        
        return a < b; // Same priority, sort alphabetically
    });
    
    return sorted;
}

inline Safe::Result<void> ModuleLoader::validateDependencies(const std::string& moduleName) const {
    // Simple dependency validation - can be enhanced later
    auto entry = getModuleEntry(moduleName);
    if (!entry || !entry->module) {
        return Safe::Result<void>::failure(
            Safe::SafeError("INVALID_MODULE", "Invalid module for dependency validation: " + moduleName)
        );
    }
    
    const auto& dependencies = entry->module->getModuleInfo().dependencies;
    for (const auto& dep : dependencies) {
        if (m_modules.find(dep) == m_modules.end()) {
            return Safe::Result<void>::failure(
                Safe::SafeError("MISSING_DEPENDENCY", "Missing dependency: " + dep + " for module: " + moduleName)
            );
        }
    }
    
    return Safe::Result<void>::success();
}

inline void ModuleLoader::setModuleState(const std::string& name, ModuleState state, const std::string& errorMessage) {
    std::unique_lock<std::shared_mutex> lock(m_modulesMutex);
    auto it = m_modules.find(name);
    if (it != m_modules.end()) {
        it->second.state = state;
        it->second.errorMessage = errorMessage;
        it->second.lastStateChange = std::chrono::system_clock::now();
    }
}

inline ModuleEntry* ModuleLoader::getModuleEntry(const std::string& name) {
    auto it = m_modules.find(name);
    return it != m_modules.end() ? &it->second : nullptr;
}

inline const ModuleEntry* ModuleLoader::getModuleEntry(const std::string& name) const {
    auto it = m_modules.find(name);
    return it != m_modules.end() ? &it->second : nullptr;
}

inline void ModuleLoader::writeGuard(std::function<void()> operation) {
    std::unique_lock<std::shared_mutex> lock(m_modulesMutex);
    operation();
}

template<typename T>
inline T ModuleLoader::readGuard(std::function<T()> operation) const {
    std::shared_lock<std::shared_mutex> lock(m_modulesMutex);
    return operation();
}

inline Safe::Result<Modules::Core::ModuleInfo> ModuleLoader::getModuleInfo(const std::string& name) const {
    return readGuard<Safe::Result<Modules::Core::ModuleInfo>>([this, &name]() {
        auto entry = getModuleEntry(name);
        if (!entry || !entry->module) {
            return Safe::Result<Modules::Core::ModuleInfo>::failure(
                Safe::SafeError("MODULE_NOT_FOUND", "Module not found: " + name)
            );
        }
        return Safe::Result<Modules::Core::ModuleInfo>::success(entry->module->getModuleInfo());
    });
}

// Stub implementations for incomplete API methods
inline Safe::Result<void> ModuleLoader::addDiscoveryPath(const std::string& path) {
    // Stub: Not yet implemented
    return Safe::Result<void>::success();
}

inline Safe::Result<void> ModuleLoader::removeDiscoveryPath(const std::string& path) {
    // Stub: Not yet implemented  
    return Safe::Result<void>::success();
}

inline Safe::Result<void> ModuleLoader::addExcludePattern(const std::string& pattern) {
    // Stub: Not yet implemented
    return Safe::Result<void>::success();
}

inline Safe::Result<void> ModuleLoader::reloadAllModules() {
    // Stub: Not yet implemented
    return Safe::Result<void>::success();
}

inline Safe::Result<void> ModuleLoader::reloadModule(const std::string& name) {
    // Stub: Not yet implemented
    return Safe::Result<void>::success();
}

// Implementation for missing API methods

inline std::vector<std::string> ModuleLoader::getDiscoveredModules() const {
    return readGuard<std::vector<std::string>>([this]() {
        std::vector<std::string> discovered;
        for (const auto& pair : m_modules) {
            if (pair.second.state == ModuleState::DISCOVERED) {
                discovered.push_back(pair.first);
            }
        }
        return discovered;
    });
}

inline std::vector<Modules::Core::ModuleInfo> ModuleLoader::getModuleInfos() const {
    return readGuard<std::vector<Modules::Core::ModuleInfo>>([this]() {
        std::vector<Modules::Core::ModuleInfo> infos;
        for (const auto& pair : m_modules) {
            if (pair.second.module) {
                infos.push_back(pair.second.module->getModuleInfo());
            }
        }
        return infos;
    });
}

inline Safe::Result<void> ModuleLoader::activateModule(const std::string& name) {
    try {
        auto entry = getModuleEntry(name);
        if (!entry || !entry->module) {
            return Safe::Result<void>::failure(
                Safe::SafeError("MODULE_NOT_FOUND", "Module not found: " + name)
            );
        }
        
        if (entry->state != ModuleState::INITIALIZED) {
            return Safe::Result<void>::failure(
                Safe::SafeError("INVALID_STATE", "Module not in initialized state: " + name)
            );
        }
        
        auto result = entry->module->activate();
        if (result.isSuccess()) {
            setModuleState(name, ModuleState::ACTIVE);
            
            // Publish module loaded event
            Core::ModuleLoadedEvent loadedEvent(name, entry->module->getModuleInfo().version);
            Core::EventSystem::getInstance().publish(loadedEvent);
        } else {
            setModuleState(name, ModuleState::ERROR, "Activation failed: " + result.getError().message);
        }
        
        return result;
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("ACTIVATION_EXCEPTION", "Exception activating module " + name + ": " + e.what())
        );
    }
}

inline Safe::Result<void> ModuleLoader::deactivateModule(const std::string& name) {
    try {
        auto entry = getModuleEntry(name);
        if (!entry || !entry->module) {
            return Safe::Result<void>::failure(
                Safe::SafeError("MODULE_NOT_FOUND", "Module not found: " + name)
            );
        }
        
        if (entry->state != ModuleState::ACTIVE) {
            return Safe::Result<void>::failure(
                Safe::SafeError("INVALID_STATE", "Module not in active state: " + name)
            );
        }
        
        auto result = entry->module->deactivate();
        if (result.isSuccess()) {
            setModuleState(name, ModuleState::INITIALIZED);
            
            // Publish module unloaded event
            Core::ModuleUnloadedEvent unloadedEvent(name);
            Core::EventSystem::getInstance().publish(unloadedEvent);
        } else {
            setModuleState(name, ModuleState::ERROR, "Deactivation failed: " + result.getError().message);
        }
        
        return result;
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("DEACTIVATION_EXCEPTION", "Exception deactivating module " + name + ": " + e.what())
        );
    }
}

inline Safe::Result<void> ModuleLoader::activateAllModules() {
    auto moduleNames = readGuard<std::vector<std::string>>([this]() {
        std::vector<std::string> names;
        for (const auto& pair : m_modules) {
            if (pair.second.state == ModuleState::INITIALIZED) {
                names.push_back(pair.first);
            }
        }
        return names;
    });
    
    int successCount = 0;
    int errorCount = 0;
    
    for (const auto& name : moduleNames) {
        auto result = activateModule(name);
        if (result.isSuccess()) {
            successCount++;
        } else {
            errorCount++;
            LOG_ERROR_STREAM("Failed to activate module " << name << ": " << result.getError().message);
        }
    }
    
    LOG_INFO_STREAM("Module activation completed. Activated: " << successCount << ", Errors: " << errorCount);
    
    if (errorCount > 0) {
        return Safe::Result<void>::failure(
            Safe::SafeError("PARTIAL_ACTIVATION_FAILURE", 
                "Some modules failed to activate (" + std::to_string(errorCount) + " errors)")
        );
    }
    
    return Safe::Result<void>::success();
}

inline Safe::Result<void> ModuleLoader::deactivateAllModules() {
    auto moduleNames = readGuard<std::vector<std::string>>([this]() {
        std::vector<std::string> names;
        for (const auto& pair : m_modules) {
            if (pair.second.state == ModuleState::ACTIVE) {
                names.push_back(pair.first);
            }
        }
        return names;
    });
    
    int successCount = 0;
    int errorCount = 0;
    
    for (const auto& name : moduleNames) {
        auto result = deactivateModule(name);
        if (result.isSuccess()) {
            successCount++;
        } else {
            errorCount++;
            LOG_ERROR_STREAM("Failed to deactivate module " << name << ": " << result.getError().message);
        }
    }
    
    LOG_INFO_STREAM("Module deactivation completed. Deactivated: " << successCount << ", Errors: " << errorCount);
    
    if (errorCount > 0) {
        return Safe::Result<void>::failure(
            Safe::SafeError("PARTIAL_DEACTIVATION_FAILURE", 
                "Some modules failed to deactivate (" + std::to_string(errorCount) + " errors)")
        );
    }
    
    return Safe::Result<void>::success();
}

inline std::vector<std::string> ModuleLoader::getModulesInErrorState() const {
    return readGuard<std::vector<std::string>>([this]() {
        std::vector<std::string> errorModules;
        for (const auto& pair : m_modules) {
            if (pair.second.state == ModuleState::ERROR) {
                errorModules.push_back(pair.first);
            }
        }
        return errorModules;
    });
}

inline Safe::Result<void> ModuleLoader::clearModuleError(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(m_modulesMutex);
    auto it = m_modules.find(name);
    if (it != m_modules.end() && it->second.state == ModuleState::ERROR) {
        it->second.state = ModuleState::DISCOVERED;
        it->second.errorMessage.clear();
        it->second.lastStateChange = std::chrono::system_clock::now();
        LOG_INFO_STREAM("Cleared error state for module: " << name);
        return Safe::Result<void>::success();
    }
    return Safe::Result<void>::failure(
        Safe::SafeError("MODULE_NOT_IN_ERROR", "Module not found or not in error state: " + name)
    );
}

inline Safe::Result<void> ModuleLoader::recoverFromErrors() {
    auto errorModules = getModulesInErrorState();
    if (errorModules.empty()) {
        return Safe::Result<void>::success();
    }
    
    int recoveredCount = 0;
    int failedCount = 0;
    
    for (const auto& name : errorModules) {
        auto clearResult = clearModuleError(name);
        if (clearResult.isSuccess()) {
            auto loadResult = loadModule(name);
            if (loadResult.isSuccess()) {
                recoveredCount++;
                LOG_INFO_STREAM("Successfully recovered module: " << name);
            } else {
                failedCount++;
                LOG_ERROR_STREAM("Failed to recover module " << name << ": " << loadResult.getError().message);
            }
        } else {
            failedCount++;
            LOG_ERROR_STREAM("Failed to clear error state for module " << name << ": " << clearResult.getError().message);
        }
    }
    
    LOG_INFO_STREAM("Recovery completed. Recovered: " << recoveredCount << ", Failed: " << failedCount);
    
    if (failedCount > 0) {
        return Safe::Result<void>::failure(
            Safe::SafeError("PARTIAL_RECOVERY_FAILURE", 
                "Some modules failed to recover (" + std::to_string(failedCount) + " errors)")
        );
    }
    
    return Safe::Result<void>::success();
}
