#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <functional>
#include <cstring>
#include <sstream>
#include <thread>
#include "../core/Logger.h"
#include "../core/SafeOperation.h"
#include "../core/EventSystem.h"
#include "../小莫控制器/RTSSApi/RTSSInterface.h"
#include "RTSSEvents.h"

namespace RTSS {

// Enhanced error types for RTSS operations
enum class RTSSErrorType {
    INITIALIZATION_ERROR,
    PROFILE_ERROR,
    PROPERTY_ERROR,
    DLL_ERROR,
    VALIDATION_ERROR,
    TIMEOUT_ERROR
};

// RTSS rendering modes
enum class RenderingMode {
    RASTER3D = 0,
    VECTOR2D = 1
};

// RTSS application detection levels
enum class AppDetectionLevel {
    NONE = 0,
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3
};

// RTSS coordinate spaces
enum class CoordinateSpace {
    FRAMEBUFFER = 0,
    FULLSCREEN = 1
};

// Profile configuration structure
struct RTSSProfileConfig {
    std::string name;
    bool enabled = true;
    RenderingMode renderingMode = RenderingMode::RASTER3D;
    AppDetectionLevel detectionLevel = AppDetectionLevel::MEDIUM;
    CoordinateSpace coordinateSpace = CoordinateSpace::FRAMEBUFFER;
    int refreshRate = 60;
    bool vsyncEnabled = false;
    std::unordered_map<std::string, std::string> customProperties;
};

// Overlay text configuration
struct OverlayTextConfig {
    std::string text;
    int x = 0;
    int y = 0;
    uint32_t color = 0xFFFFFFFF; // ARGB format
    int fontSize = 12;
    std::string fontName = "Arial";
    bool visible = true;
    int layer = 0;
};

/**
 * @brief Enhanced RTSS interface with modern C++ design
 * 
 * Provides thread-safe, RAII-based wrapper around the RTSS API with:
 * - Comprehensive error handling and logging
 * - Event-driven architecture for state changes
 * - Profile and overlay management
 * - Integration with core infrastructure
 */
class EnhancedRTSSInterface {
private:
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> profileLoaded_{false};
    
    // RTSS handles and state
    void* rtssHandle_ = nullptr;
    std::string currentProfile_;
    std::unordered_map<std::string, OverlayTextConfig> overlayTexts_;
    
    // Event system integration
    Core::SubscriptionId rtssEventSubscription_;
    
    // Configuration cache
    RTSSProfileConfig cachedConfig_;
    
    // Performance monitoring
    std::chrono::steady_clock::time_point lastUpdateTime_;
    std::atomic<size_t> updateCount_{0};
    
public:
    /**
     * @brief Constructor - initializes RTSS interface
     */
    EnhancedRTSSInterface();
    
    /**
     * @brief Destructor - ensures proper cleanup
     */
    ~EnhancedRTSSInterface();
    
    // Non-copyable, non-movable for resource safety
    EnhancedRTSSInterface(const EnhancedRTSSInterface&) = delete;
    EnhancedRTSSInterface& operator=(const EnhancedRTSSInterface&) = delete;
    EnhancedRTSSInterface(EnhancedRTSSInterface&&) = delete;
    EnhancedRTSSInterface& operator=(EnhancedRTSSInterface&&) = delete;
    
    /**
     * @brief Initialize RTSS interface
     * @return Result indicating success or failure with error details
     */
    Core::Safe::Result<void> initialize();
    
    /**
     * @brief Shutdown RTSS interface safely
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> shutdown();
    
    /**
     * @brief Load RTSS profile by name
     * @param profileName Name of the profile to load
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> loadProfile(const std::string& profileName);
    
    /**
     * @brief Get current profile configuration
     * @return Result containing profile config or error
     */
    Core::Safe::Result<RTSSProfileConfig> getProfileConfig() const;
    
    /**
     * @brief Update profile configuration
     * @param config New profile configuration
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> updateProfileConfig(const RTSSProfileConfig& config);
    
    /**
     * @brief Add or update overlay text
     * @param id Unique identifier for the overlay text
     * @param config Text configuration
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> setOverlayText(const std::string& id, const OverlayTextConfig& config);
    
    /**
     * @brief Remove overlay text by ID
     * @param id Unique identifier of the overlay text to remove
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> removeOverlayText(const std::string& id);
    
    /**
     * @brief Get overlay text configuration
     * @param id Unique identifier of the overlay text
     * @return Result containing text config or error
     */
    Core::Safe::Result<OverlayTextConfig> getOverlayText(const std::string& id) const;
    
    /**
     * @brief Clear all overlay texts
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> clearAllOverlays();
    
    /**
     * @brief Check if RTSS is initialized and ready
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const { return initialized_.load(); }
    
    /**
     * @brief Check if a profile is currently loaded
     * @return true if profile loaded, false otherwise
     */
    bool isProfileLoaded() const { return profileLoaded_.load(); }
    
    /**
     * @brief Get current profile name
     * @return Current profile name or empty string if none loaded
     */
    std::string getCurrentProfile() const;
    
    /**
     * @brief Get performance statistics
     * @return String containing performance info
     */
    std::string getPerformanceStats() const;
    
    /**
     * @brief Enable or disable RTSS overlay
     * @param enabled true to enable, false to disable
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> setOverlayEnabled(bool enabled);
    
    /**
     * @brief Update overlay position
     * @param id Overlay text ID
     * @param x New X coordinate
     * @param y New Y coordinate
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> updateOverlayPosition(const std::string& id, int x, int y);
    
    /**
     * @brief Update overlay color
     * @param id Overlay text ID
     * @param color New color in ARGB format
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> updateOverlayColor(const std::string& id, uint32_t color);

private:
    /**
     * @brief Internal initialization helper
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> initializeInternal();
    
    /**
     * @brief Internal cleanup helper
     */
    void cleanupInternal();
    
    /**
     * @brief Validate RTSS DLL availability
     * @return Result indicating success or failure
     */
    Core::Safe::Result<void> validateRTSSDLL();
    
    /**
     * @brief Subscribe to RTSS-related events
     */
    void subscribeToEvents();
    
    /**
     * @brief Unsubscribe from events
     */
    void unsubscribeFromEvents();
    
    /**
     * @brief Handle RTSS events
     * @param event RTSS event to handle
     */
    void handleRTSSEvent(const RTSSStateChangedEvent& event);
    
    /**
     * @brief Convert error type to string
     * @param errorType Error type to convert
     * @return String representation of error
     */
    std::string errorTypeToString(RTSSErrorType errorType) const;
    
    /**
     * @brief Update performance counters
     */
    void updatePerformanceCounters();
    
    /**
     * @brief Validate overlay text configuration
     * @param config Configuration to validate
     * @return Result indicating validity
     */
    Core::Safe::Result<void> validateOverlayConfig(const OverlayTextConfig& config) const;
    
    /**
     * @brief Publish RTSS state change event
     * @param state New RTSS state
     */
    void publishStateChangeEvent(RTSSState state);
};

/**
 * @brief Singleton manager for RTSS interface
 * 
 * Provides global access to RTSS functionality with thread-safe initialization
 */
class RTSSManager {
private:
    static std::unique_ptr<EnhancedRTSSInterface> instance_;
    static std::mutex instanceMutex_;
    static std::atomic<bool> initialized_;
    
public:
    /**
     * @brief Get singleton instance
     * @return Reference to RTSS interface instance
     */
    static EnhancedRTSSInterface& getInstance();
    
    /**
     * @brief Initialize RTSS manager
     * @return Result indicating success or failure
     */
    static Core::Safe::Result<void> initialize();
    
    /**
     * @brief Shutdown RTSS manager
     * @return Result indicating success or failure
     */
    static Core::Safe::Result<void> shutdown();
    
    /**
     * @brief Check if manager is initialized
     * @return true if initialized, false otherwise
     */
    static bool isInitialized() { return initialized_.load(); }
};

} // namespace RTSS