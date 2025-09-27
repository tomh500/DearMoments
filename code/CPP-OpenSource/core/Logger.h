#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include "Constants.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace Core {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

class Logger {
private:
    
    std::mutex logMutex;
    std::ofstream logFile;
    LogLevel currentLevel;
    bool enableFileLogging;
    bool enableConsoleLogging;
    std::string logFilePath;
    size_t maxFileSize;
    
    Logger() : currentLevel(LogLevel::INFO), enableFileLogging(true), 
               enableConsoleLogging(true), maxFileSize(Constants::Application::MAX_LOG_FILE_SIZE) {
        try {
            // Create logs directory if it doesn't exist
            std::filesystem::create_directories(Constants::Paths::LOG_DIR);
            
            // Generate timestamp-based log file name
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << Constants::Paths::LOG_DIR << "/" << Constants::Application::LOG_FILE_PREFIX 
               << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".log";
            logFilePath = ss.str();
            
            if (enableFileLogging) {
                logFile.open(logFilePath, std::ios::app);
                if (logFile.is_open()) {
                    logFile << "\n=== Logger Session Started ===" << std::endl;
                }
            }
        }
        catch (const std::exception& e) {
            enableFileLogging = false;
            std::cerr << "[Logger Warning] Failed to initialize file logging: " << e.what() << std::endl;
        }
    }
    
    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
    
    std::string levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::CRITICAL: return "CRITICAL";
            default: return "UNKNOWN";
        }
    }
    
    void setConsoleColor(LogLevel level) const {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole == INVALID_HANDLE_VALUE) return;
        
        WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Default white
        switch (level) {
            case LogLevel::DEBUG: color = FOREGROUND_BLUE; break;
            case LogLevel::INFO: color = FOREGROUND_GREEN; break;
            case LogLevel::WARNING: color = FOREGROUND_RED | FOREGROUND_GREEN; break;
            case LogLevel::ERROR: color = FOREGROUND_RED; break;
            case LogLevel::CRITICAL: color = FOREGROUND_RED | FOREGROUND_INTENSITY; break;
        }
        SetConsoleTextAttribute(hConsole, color);
#endif
    }
    
    void resetConsoleColor() const {
#ifdef _WIN32
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hConsole != INVALID_HANDLE_VALUE) {
            SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
#endif
    }
    
    void rotateLogFileIfNeeded() {
        if (!logFile.is_open()) return;
        
        logFile.seekp(0, std::ios::end);
        if (logFile.tellp() > static_cast<std::streampos>(maxFileSize)) {
            logFile.close();
            
            try {
                // Rotate log file
                std::string backupPath = logFilePath + ".bak";
                std::filesystem::rename(logFilePath, backupPath);
                
                // Open new log file
                logFile.open(logFilePath, std::ios::out);
                if (logFile.is_open()) {
                    logFile << "=== Logger Session Started (Rotated) ===" << std::endl;
                }
            }
            catch (const std::exception& e) {
                enableFileLogging = false;
                std::cerr << "[Logger Warning] Failed to rotate log file: " << e.what() << std::endl;
                // Try to reopen original file in append mode
                logFile.open(logFilePath, std::ios::app);
            }
        }
    }

public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    ~Logger() {
        if (logFile.is_open()) {
            logFile << "=== Logger Session Ended ===" << std::endl;
            logFile.close();
        }
    }
    
    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(logMutex);
        currentLevel = level;
    }
    
    void setFileLogging(bool enabled) {
        std::lock_guard<std::mutex> lock(logMutex);
        enableFileLogging = enabled;
        if (!enabled && logFile.is_open()) {
            logFile.close();
        } else if (enabled && !logFile.is_open()) {
            logFile.open(logFilePath, std::ios::app);
        }
    }
    
    void setConsoleLogging(bool enabled) {
        std::lock_guard<std::mutex> lock(logMutex);
        enableConsoleLogging = enabled;
    }
    
    void setMaxFileSize(size_t size) {
        std::lock_guard<std::mutex> lock(logMutex);
        maxFileSize = size;
    }
    
    // Override max file size from default constant
    void setMaxFileSize(size_t size, bool override_default) {
        std::lock_guard<std::mutex> lock(logMutex);
        maxFileSize = override_default ? size : Constants::Application::MAX_LOG_FILE_SIZE;
    }
    
    void log(LogLevel level, const std::string& message, const std::string& file = "", int line = 0) {
        if (level < currentLevel) return;
        
        std::lock_guard<std::mutex> lock(logMutex);
        
        std::stringstream logEntry;
        logEntry << "[" << getCurrentTimestamp() << "] ";
        logEntry << "[" << levelToString(level) << "] ";
        
        if (!file.empty() && line > 0) {
            std::string filename = std::filesystem::path(file).filename().string();
            logEntry << "[" << filename << ":" << line << "] ";
        }
        
        logEntry << message;
        
        // Console output
        if (enableConsoleLogging) {
            setConsoleColor(level);
            std::cout << logEntry.str() << std::endl;
            resetConsoleColor();
        }
        
        // File output
        if (enableFileLogging && logFile.is_open()) {
            rotateLogFileIfNeeded();
            logFile << logEntry.str() << std::endl;
            logFile.flush();
        }
    }
    
    // Convenience methods
    void debug(const std::string& message, const std::string& file = "", int line = 0) { log(LogLevel::DEBUG, message, file, line); }
    void info(const std::string& message, const std::string& file = "", int line = 0) { log(LogLevel::INFO, message, file, line); }
    void warning(const std::string& message, const std::string& file = "", int line = 0) { log(LogLevel::WARNING, message, file, line); }
    void error(const std::string& message, const std::string& file = "", int line = 0) { log(LogLevel::ERROR, message, file, line); }
    void critical(const std::string& message, const std::string& file = "", int line = 0) { log(LogLevel::CRITICAL, message, file, line); }
};


} // namespace Core

// Convenience macros for easy logging
#define LOG_DEBUG(msg) Core::Logger::getInstance().debug(msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Core::Logger::getInstance().info(msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Core::Logger::getInstance().warning(msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Core::Logger::getInstance().error(msg, __FILE__, __LINE__)
#define LOG_CRITICAL(msg) Core::Logger::getInstance().critical(msg, __FILE__, __LINE__)

// Stream-style logging macros
#define LOG_DEBUG_STREAM(stream) do { std::stringstream _ss; _ss << stream; LOG_DEBUG(_ss.str()); } while(0)
#define LOG_INFO_STREAM(stream) do { std::stringstream _ss; _ss << stream; LOG_INFO(_ss.str()); } while(0)
#define LOG_WARNING_STREAM(stream) do { std::stringstream _ss; _ss << stream; LOG_WARNING(_ss.str()); } while(0)
#define LOG_ERROR_STREAM(stream) do { std::stringstream _ss; _ss << stream; LOG_ERROR(_ss.str()); } while(0)
#define LOG_CRITICAL_STREAM(stream) do { std::stringstream _ss; _ss << stream; LOG_CRITICAL(_ss.str()); } while(0)

#endif // LOGGER_H