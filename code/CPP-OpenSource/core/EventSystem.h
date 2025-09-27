#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <typeinfo>
#include <string>
#include <chrono>
#include <atomic>
#include <exception>
#include "Logger.h"
#include "SafeOperation.h"

namespace Core {

/**
 * Event category enumeration for filtering and routing
 */
enum class EventCategory {
    SYSTEM,     // System-level events
    MODULE,     // Module lifecycle events
    GAME,       // CS2 game state events
    AUDIO,      // Audio system events
    CONFIG,     // Configuration events
    USER,       // User interaction events
    ERROR       // Error and exception events
};

/**
 * Event metadata structure with common event information
 */
struct EventMetadata {
    std::chrono::system_clock::time_point timestamp;
    std::string source;
    std::string context;
    EventCategory category;
    
    EventMetadata(const std::string& src = "", EventCategory cat = EventCategory::SYSTEM, const std::string& ctx = "")
        : timestamp(std::chrono::system_clock::now()), source(src), context(ctx), category(cat) {}
};

/**
 * Abstract base class for all events in the system
 * Provides common metadata and type information
 */
class Event {
private:
    EventMetadata m_metadata;
    
public:
    explicit Event(const EventMetadata& metadata = EventMetadata()) : m_metadata(metadata) {}
    virtual ~Event() = default;
    
    const EventMetadata& getMetadata() const { return m_metadata; }
    EventCategory getCategory() const { return m_metadata.category; }
    const std::string& getSource() const { return m_metadata.source; }
    const std::string& getContext() const { return m_metadata.context; }
    std::chrono::system_clock::time_point getTimestamp() const { return m_metadata.timestamp; }
    
    virtual std::string getEventType() const = 0;
    virtual std::string toString() const = 0;
};

/**
 * CS2-specific event types for the game system
 */

// Module Events
class ModuleLoadedEvent : public Event {
private:
    std::string m_moduleName;
    std::string m_moduleVersion;
    
public:
    ModuleLoadedEvent(const std::string& name, const std::string& version, const std::string& source = "ModuleLoader")
        : Event(EventMetadata(source, EventCategory::MODULE, "Module loaded")), m_moduleName(name), m_moduleVersion(version) {}
        
    const std::string& getModuleName() const { return m_moduleName; }
    const std::string& getModuleVersion() const { return m_moduleVersion; }
    
    std::string getEventType() const override { return "ModuleLoadedEvent"; }
    std::string toString() const override { 
        return "ModuleLoadedEvent: " + m_moduleName + " v" + m_moduleVersion; 
    }
};

class ModuleUnloadedEvent : public Event {
private:
    std::string m_moduleName;
    
public:
    explicit ModuleUnloadedEvent(const std::string& name, const std::string& source = "ModuleLoader")
        : Event(EventMetadata(source, EventCategory::MODULE, "Module unloaded")), m_moduleName(name) {}
        
    const std::string& getModuleName() const { return m_moduleName; }
    
    std::string getEventType() const override { return "ModuleUnloadedEvent"; }
    std::string toString() const override { return "ModuleUnloadedEvent: " + m_moduleName; }
};

class ModuleErrorEvent : public Event {
private:
    std::string m_moduleName;
    std::string m_errorMessage;
    
public:
    ModuleErrorEvent(const std::string& name, const std::string& error, const std::string& source = "ModuleLoader")
        : Event(EventMetadata(source, EventCategory::ERROR, "Module error")), m_moduleName(name), m_errorMessage(error) {}
        
    const std::string& getModuleName() const { return m_moduleName; }
    const std::string& getErrorMessage() const { return m_errorMessage; }
    
    std::string getEventType() const override { return "ModuleErrorEvent"; }
    std::string toString() const override { 
        return "ModuleErrorEvent: " + m_moduleName + " - " + m_errorMessage; 
    }
};

// Game State Events
class GamePhaseChangedEvent : public Event {
private:
    std::string m_previousPhase;
    std::string m_currentPhase;
    
public:
    GamePhaseChangedEvent(const std::string& prevPhase, const std::string& currentPhase, 
                         std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : Event(EventMetadata("GSI", EventCategory::GAME, "Game phase changed")), 
          m_previousPhase(prevPhase), m_currentPhase(currentPhase) {}
          
    const std::string& getPreviousPhase() const { return m_previousPhase; }
    const std::string& getCurrentPhase() const { return m_currentPhase; }
    
    std::string getEventType() const override { return "GamePhaseChangedEvent"; }
    std::string toString() const override { 
        return "GamePhaseChangedEvent: " + m_previousPhase + " -> " + m_currentPhase; 
    }
};

class PlayerKillEvent : public Event {
private:
    int m_killCount;
    int m_totalKills;
    std::string m_playerTeam;
    
public:
    PlayerKillEvent(int killCount, int totalKills, const std::string& team)
        : Event(EventMetadata("GSI", EventCategory::GAME, "Player kill")), 
          m_killCount(killCount), m_totalKills(totalKills), m_playerTeam(team) {}
          
    int getKillCount() const { return m_killCount; }
    int getTotalKills() const { return m_totalKills; }
    const std::string& getPlayerTeam() const { return m_playerTeam; }
    
    std::string getEventType() const override { return "PlayerKillEvent"; }
    std::string toString() const override { 
        return "PlayerKillEvent: +" + std::to_string(m_killCount) + " kills (total: " + std::to_string(m_totalKills) + ")"; 
    }
};

class BombPlantedEvent : public Event {
public:
    explicit BombPlantedEvent(std::chrono::system_clock::time_point plantTime = std::chrono::system_clock::now())
        : Event(EventMetadata("GSI", EventCategory::GAME, "Bomb planted")) {}
        
    std::string getEventType() const override { return "BombPlantedEvent"; }
    std::string toString() const override { return "BombPlantedEvent: Bomb planted"; }
};

class BombDefusedEvent : public Event {
public:
    explicit BombDefusedEvent(std::chrono::system_clock::time_point defuseTime = std::chrono::system_clock::now())
        : Event(EventMetadata("GSI", EventCategory::GAME, "Bomb defused")) {}
        
    std::string getEventType() const override { return "BombDefusedEvent"; }
    std::string toString() const override { return "BombDefusedEvent: Bomb defused"; }
};

// Audio Events
class SoundPlayedEvent : public Event {
private:
    std::string m_soundPath;
    float m_volume;
    int m_channel;
    
public:
    SoundPlayedEvent(const std::string& soundPath, float volume, int channel)
        : Event(EventMetadata("AudioSystem", EventCategory::AUDIO, "Sound played")), 
          m_soundPath(soundPath), m_volume(volume), m_channel(channel) {}
          
    const std::string& getSoundPath() const { return m_soundPath; }
    float getVolume() const { return m_volume; }
    int getChannel() const { return m_channel; }
    
    std::string getEventType() const override { return "SoundPlayedEvent"; }
    std::string toString() const override { 
        return "SoundPlayedEvent: " + m_soundPath + " (vol: " + std::to_string(m_volume) + ")"; 
    }
};

class SoundStoppedEvent : public Event {
private:
    int m_channel;
    
public:
    explicit SoundStoppedEvent(int channel)
        : Event(EventMetadata("AudioSystem", EventCategory::AUDIO, "Sound stopped")), m_channel(channel) {}
        
    int getChannel() const { return m_channel; }
    
    std::string getEventType() const override { return "SoundStoppedEvent"; }
    std::string toString() const override { 
        return "SoundStoppedEvent: channel " + std::to_string(m_channel); 
    }
};

// Configuration Events
class ConfigChangedEvent : public Event {
private:
    std::string m_configPath;
    std::string m_changeType;
    
public:
    ConfigChangedEvent(const std::string& configPath, const std::string& changeType)
        : Event(EventMetadata("ConfigSystem", EventCategory::CONFIG, "Configuration changed")), 
          m_configPath(configPath), m_changeType(changeType) {}
          
    const std::string& getConfigPath() const { return m_configPath; }
    const std::string& getChangeType() const { return m_changeType; }
    
    std::string getEventType() const override { return "ConfigChangedEvent"; }
    std::string toString() const override { 
        return "ConfigChangedEvent: " + m_configPath + " (" + m_changeType + ")"; 
    }
};

// System Events
class SystemStartupEvent : public Event {
public:
    SystemStartupEvent() : Event(EventMetadata("System", EventCategory::SYSTEM, "System startup")) {}
    
    std::string getEventType() const override { return "SystemStartupEvent"; }
    std::string toString() const override { return "SystemStartupEvent: System started"; }
};

class SystemShutdownEvent : public Event {
public:
    SystemShutdownEvent() : Event(EventMetadata("System", EventCategory::SYSTEM, "System shutdown")) {}
    
    std::string getEventType() const override { return "SystemShutdownEvent"; }
    std::string toString() const override { return "SystemShutdownEvent: System shutting down"; }
};

class ErrorEvent : public Event {
private:
    std::string m_errorType;
    std::string m_errorMessage;
    
public:
    ErrorEvent(const std::string& errorType, const std::string& message, const std::string& source)
        : Event(EventMetadata(source, EventCategory::ERROR, "Error occurred")), 
          m_errorType(errorType), m_errorMessage(message) {}
          
    const std::string& getErrorType() const { return m_errorType; }
    const std::string& getErrorMessage() const { return m_errorMessage; }
    
    std::string getEventType() const override { return "ErrorEvent"; }
    std::string toString() const override { 
        return "ErrorEvent: " + m_errorType + " - " + m_errorMessage; 
    }
};

/**
 * Subscription management
 */
using SubscriptionId = size_t;

template<typename T>
using EventHandler = std::function<void(const T&)>;

/**
 * Type-safe event subscription entry
 */
class SubscriptionEntry {
private:
    std::function<void(const Event&)> m_handler;
    std::type_index m_eventType;
    SubscriptionId m_id;
    std::chrono::system_clock::time_point m_createdAt;
    
public:
    template<typename T>
    SubscriptionEntry(SubscriptionId id, EventHandler<T> handler)
        : m_id(id), m_eventType(std::type_index(typeid(T))), m_createdAt(std::chrono::system_clock::now()) {
        m_handler = [handler](const Event& event) {
            try {
                const T& typedEvent = dynamic_cast<const T&>(event);
                handler(typedEvent);
            } catch (const std::bad_cast& e) {
                LOG_ERROR_STREAM("Event handler type mismatch: " << e.what());
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("Exception in event handler: " << e.what());
            }
        };
    }
    
    void invoke(const Event& event) const { m_handler(event); }
    std::type_index getEventType() const { return m_eventType; }
    SubscriptionId getId() const { return m_id; }
    std::chrono::system_clock::time_point getCreatedAt() const { return m_createdAt; }
};

/**
 * Comprehensive event system for loose coupling and communication between components
 * Thread-safe singleton implementation with type-safe event handling
 */
class EventSystem {
private:
    static EventSystem* s_instance;
    static std::mutex s_instanceMutex;
    
    std::unordered_map<std::type_index, std::vector<std::unique_ptr<SubscriptionEntry>>> m_subscriptions;
    mutable std::shared_mutex m_subscriptionsMutex;
    
    std::atomic<SubscriptionId> m_nextSubscriptionId{1};
    std::atomic<size_t> m_totalEventsPublished{0};
    std::atomic<size_t> m_totalSubscriptions{0};
    
    // Performance tracking
    mutable std::mutex m_statsMutex;
    std::unordered_map<std::string, size_t> m_eventCounts;
    std::unordered_map<std::string, std::chrono::nanoseconds> m_handlerExecutionTimes;
    
    // Private constructor for singleton
    EventSystem();

public:
    // Singleton access
    static EventSystem& getInstance();
    
    // Destructor
    ~EventSystem();
    
    // Non-copyable
    EventSystem(const EventSystem&) = delete;
    EventSystem& operator=(const EventSystem&) = delete;

    // Type-safe event subscription
    template<typename T>
    SubscriptionId subscribe(EventHandler<T> handler);
    
    // Convenient subscription syntax
    template<typename T>
    SubscriptionId on(EventHandler<T> handler) { return subscribe<T>(handler); }
    
    // Event unsubscription
    template<typename T>
    bool unsubscribe(SubscriptionId subscriptionId);
    
    // Generic unsubscription by ID only (iterates all buckets)
    bool unsubscribe(SubscriptionId subscriptionId);
    
    // Convenient unsubscription syntax
    template<typename T>
    bool off(SubscriptionId subscriptionId) { return unsubscribe<T>(subscriptionId); }
    
    // Type-safe event publishing
    template<typename T>
    void publish(const T& event);
    
    // Convenient publishing syntax
    template<typename T>
    void emit(const T& event) { publish<T>(event); }
    
    // Subscription management
    size_t getSubscriptionCount() const;
    template<typename T>
    size_t getSubscriptionCount() const;
    
    // Statistics and monitoring
    size_t getTotalEventsPublished() const { return m_totalEventsPublished.load(); }
    size_t getTotalSubscriptions() const { return m_totalSubscriptions.load(); }
    std::unordered_map<std::string, size_t> getEventCounts() const;
    
    // Event system health
    bool isHealthy() const;
    void clearStatistics();
    
    // Cleanup
    void cleanup();

private:
    void recordEventCount(const std::string& eventType);
    void recordHandlerExecutionTime(const std::string& eventType, std::chrono::nanoseconds duration);
    std::vector<SubscriptionEntry*> getSubscriptionsForType(std::type_index typeIndex) const;
};

// Singleton Implementation
inline EventSystem* EventSystem::s_instance = nullptr;
inline std::mutex EventSystem::s_instanceMutex;

inline EventSystem::EventSystem() {
    LOG_INFO("EventSystem instance created");
}

inline EventSystem::~EventSystem() {
    cleanup();
    LOG_INFO("EventSystem instance destroyed");
}

inline EventSystem& EventSystem::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = new EventSystem();
    }
    return *s_instance;
}

// Template Implementation

template<typename T>
inline SubscriptionId EventSystem::subscribe(EventHandler<T> handler) {
    static_assert(std::is_base_of_v<Event, T>, "T must be derived from Event");
    
    SubscriptionId id = m_nextSubscriptionId.fetch_add(1);
    auto subscription = std::make_unique<SubscriptionEntry>(id, handler);
    
    {
        std::unique_lock<std::shared_mutex> lock(m_subscriptionsMutex);
        std::type_index typeIndex(typeid(T));
        m_subscriptions[typeIndex].push_back(std::move(subscription));
        m_totalSubscriptions.fetch_add(1);
    }
    
    LOG_DEBUG_STREAM("Subscribed to event type: " << typeid(T).name() << " (ID: " << id << ")");
    return id;
}

template<typename T>
inline bool EventSystem::unsubscribe(SubscriptionId subscriptionId) {
    static_assert(std::is_base_of_v<Event, T>, "T must be derived from Event");
    
    std::unique_lock<std::shared_mutex> lock(m_subscriptionsMutex);
    std::type_index typeIndex(typeid(T));
    
    auto it = m_subscriptions.find(typeIndex);
    if (it != m_subscriptions.end()) {
        auto& subscriptions = it->second;
        auto subIt = std::remove_if(subscriptions.begin(), subscriptions.end(),
            [subscriptionId](const std::unique_ptr<SubscriptionEntry>& sub) {
                return sub->getId() == subscriptionId;
            });
            
        if (subIt != subscriptions.end()) {
            subscriptions.erase(subIt, subscriptions.end());
            m_totalSubscriptions.fetch_sub(1);
            LOG_DEBUG_STREAM("Unsubscribed from event type: " << typeid(T).name() << " (ID: " << subscriptionId << ")");
            return true;
        }
    }
    
    LOG_WARNING_STREAM("Failed to unsubscribe: subscription ID " << subscriptionId << " not found for type " << typeid(T).name());
    return false;
}

inline bool EventSystem::unsubscribe(SubscriptionId subscriptionId) {
    std::unique_lock<std::shared_mutex> lock(m_subscriptionsMutex);
    
    // Iterate through all event type buckets to find and remove the subscription
    for (auto& pair : m_subscriptions) {
        auto& subscriptions = pair.second;
        auto subIt = std::remove_if(subscriptions.begin(), subscriptions.end(),
            [subscriptionId](const std::unique_ptr<SubscriptionEntry>& sub) {
                return sub->getId() == subscriptionId;
            });
            
        if (subIt != subscriptions.end()) {
            subscriptions.erase(subIt, subscriptions.end());
            m_totalSubscriptions.fetch_sub(1);
            LOG_DEBUG_STREAM("Generic unsubscribed subscription ID: " << subscriptionId);
            return true;
        }
    }
    
    LOG_WARNING_STREAM("Failed to unsubscribe: subscription ID " << subscriptionId << " not found in any event type");
    return false;
}

template<typename T>
inline void EventSystem::publish(const T& event) {
    static_assert(std::is_base_of_v<Event, T>, "T must be derived from Event");
    
    std::type_index typeIndex(typeid(T));
    auto subscriptions = getSubscriptionsForType(typeIndex);
    
    if (subscriptions.empty()) {
        return; // No subscribers, early exit
    }
    
    m_totalEventsPublished.fetch_add(1);
    recordEventCount(event.getEventType());
    
    LOG_DEBUG_STREAM("Publishing event: " << event.toString() << " to " << subscriptions.size() << " subscribers");
    
    // Execute all handlers
    for (auto* subscription : subscriptions) {
        try {
            auto startTime = std::chrono::high_resolution_clock::now();
            subscription->invoke(event);
            auto endTime = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
            recordHandlerExecutionTime(event.getEventType(), duration);
            
        } catch (const std::exception& e) {
            LOG_ERROR_STREAM("Exception in event handler for " << event.getEventType() << ": " << e.what());
        }
    }
}

template<typename T>
inline size_t EventSystem::getSubscriptionCount() const {
    static_assert(std::is_base_of_v<Event, T>, "T must be derived from Event");
    
    std::shared_lock<std::shared_mutex> lock(m_subscriptionsMutex);
    std::type_index typeIndex(typeid(T));
    auto it = m_subscriptions.find(typeIndex);
    return it != m_subscriptions.end() ? it->second.size() : 0;
}

// Private Implementation

inline std::vector<SubscriptionEntry*> EventSystem::getSubscriptionsForType(std::type_index typeIndex) const {
    std::shared_lock<std::shared_mutex> lock(m_subscriptionsMutex);
    
    std::vector<SubscriptionEntry*> result;
    auto it = m_subscriptions.find(typeIndex);
    if (it != m_subscriptions.end()) {
        for (const auto& subscription : it->second) {
            result.push_back(subscription.get());
        }
    }
    return result;
}

inline void EventSystem::recordEventCount(const std::string& eventType) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_eventCounts[eventType]++;
}

inline void EventSystem::recordHandlerExecutionTime(const std::string& eventType, std::chrono::nanoseconds duration) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_handlerExecutionTimes[eventType] += duration;
}

inline size_t EventSystem::getSubscriptionCount() const {
    std::shared_lock<std::shared_mutex> lock(m_subscriptionsMutex);
    size_t total = 0;
    for (const auto& pair : m_subscriptions) {
        total += pair.second.size();
    }
    return total;
}

inline std::unordered_map<std::string, size_t> EventSystem::getEventCounts() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_eventCounts;
}

inline bool EventSystem::isHealthy() const {
    // Simple health check - can be enhanced
    return getSubscriptionCount() < 10000 && // Reasonable subscription limit
           getTotalEventsPublished() >= 0;   // Basic sanity check
}

inline void EventSystem::clearStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_eventCounts.clear();
    m_handlerExecutionTimes.clear();
    m_totalEventsPublished.store(0);
}

inline void EventSystem::cleanup() {
    LOG_INFO("Starting EventSystem cleanup");
    
    {
        std::unique_lock<std::shared_mutex> lock(m_subscriptionsMutex);
        m_subscriptions.clear();
        m_totalSubscriptions.store(0);
    }
    
    clearStatistics();
    LOG_INFO("EventSystem cleanup completed");
}

} // namespace Core
