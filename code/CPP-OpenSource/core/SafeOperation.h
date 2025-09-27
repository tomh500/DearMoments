#ifndef SAFE_OPERATION_H
#define SAFE_OPERATION_H

#include <string>
#include <memory>
#include <functional>
#include <fstream>
#include <filesystem>
#include <vector>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif
#include <SDL_mixer.h>
#include <simdjson.h>
#include "core/Logger.h"

namespace Core {
namespace Safe {

enum class OperationError {
    SUCCESS = 0,
    FILE_ERROR,
    JSON_ERROR,
    NETWORK_ERROR,
    VALIDATION_ERROR,
    REGISTRY_ERROR,
    AUDIO_ERROR,
    MEMORY_ERROR,
    UNKNOWN_ERROR
};

struct ErrorInfo {
    OperationError code;
    std::string message;
    std::string context;
    int systemErrorCode;
    
    ErrorInfo(OperationError c = OperationError::SUCCESS, 
              const std::string& msg = "", 
              const std::string& ctx = "",
              int sysCode = 0)
        : code(c), message(msg), context(ctx), systemErrorCode(sysCode) {}
    
    std::string toString() const {
        std::string result = message;
        if (!context.empty()) {
            result += " (Context: " + context + ")";
        }
        if (systemErrorCode != 0) {
            result += " (System Error: " + std::to_string(systemErrorCode) + ")";
        }
        return result;
    }
};

template<typename T>
class Result {
private:
    bool success_;
    T value_;
    ErrorInfo error_;
    
public:
    // Constructors
    Result(const T& value) : success_(true), value_(value), error_() {}
    Result(T&& value) : success_(true), value_(std::move(value)), error_() {}
    Result(const ErrorInfo& error) : success_(false), value_(), error_(error) {}
    
    // Copy and move constructors
    Result(const Result& other) = default;
    Result(Result&& other) noexcept = default;
    Result& operator=(const Result& other) = default;
    Result& operator=(Result&& other) noexcept = default;
    
    // Status checks
    bool isSuccess() const { return success_; }
    bool isError() const { return !success_; }
    
    // Value accessors
    const T& getValue() const {
        if (!success_) {
            throw std::runtime_error("Attempted to get value from error result: " + error_.toString());
        }
        return value_;
    }
    
    T& getValue() {
        if (!success_) {
            throw std::runtime_error("Attempted to get value from error result: " + error_.toString());
        }
        return value_;
    }
    
    const ErrorInfo& getError() const {
        return error_;
    }
    
    // Monadic operations
    template<typename U>
    Result<U> map(std::function<U(const T&)> func) const {
        if (success_) {
            try {
                return Result<U>(func(value_));
            } catch (const std::exception& e) {
                return Result<U>(ErrorInfo(OperationError::UNKNOWN_ERROR, e.what(), "map operation"));
            }
        }
        return Result<U>(error_);
    }
    
    template<typename U>
    Result<U> flatMap(std::function<Result<U>(const T&)> func) const {
        if (success_) {
            try {
                return func(value_);
            } catch (const std::exception& e) {
                return Result<U>(ErrorInfo(OperationError::UNKNOWN_ERROR, e.what(), "flatMap operation"));
            }
        }
        return Result<U>(error_);
    }
    
    // Utility methods
    T getOrDefault(const T& defaultValue) const {
        return success_ ? value_ : defaultValue;
    }
    
    template<typename ExceptionType>
    T getOrThrow() const {
        if (!success_) {
            throw ExceptionType(error_.toString());
        }
        return value_;
    }
};

// Specialization for void
template<>
class Result<void> {
private:
    bool success_;
    ErrorInfo error_;
    
public:
    Result() : success_(true), error_() {}
    Result(const ErrorInfo& error) : success_(false), error_(error) {}
    
    bool isSuccess() const { return success_; }
    bool isError() const { return !success_; }
    
    const ErrorInfo& getError() const { return error_; }
    
    void getOrThrow() const {
        if (!success_) {
            throw std::runtime_error(error_.toString());
        }
    }
};

// Helper functions for creating common error types
inline ErrorInfo createFileError(const std::string& message, const std::string& context = "", int sysCode = 0) {
    return ErrorInfo(OperationError::FILE_ERROR, message, context, sysCode);
}

inline ErrorInfo createJSONError(const std::string& message, const std::string& context = "") {
    return ErrorInfo(OperationError::JSON_ERROR, message, context);
}

inline ErrorInfo createNetworkError(const std::string& message, const std::string& context = "", int sysCode = 0) {
    return ErrorInfo(OperationError::NETWORK_ERROR, message, context, sysCode);
}

inline ErrorInfo createValidationError(const std::string& message, const std::string& context = "") {
    return ErrorInfo(OperationError::VALIDATION_ERROR, message, context);
}

inline ErrorInfo createRegistryError(const std::string& message, const std::string& context = "", int sysCode = 0) {
    return ErrorInfo(OperationError::REGISTRY_ERROR, message, context, sysCode);
}

inline ErrorInfo createAudioError(const std::string& message, const std::string& context = "") {
    return ErrorInfo(OperationError::AUDIO_ERROR, message, context);
}

// Safe File Operations
class SafeFileOperation {
public:
    static Result<std::string> loadFile(const std::string& path) {
        try {
            if (!std::filesystem::exists(path)) {
                Logger::getInstance().log(LogLevel::ERROR, "File does not exist: " + path);
                return Result<std::string>(createFileError("File does not exist", path));
            }
            
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                Logger::getInstance().log(LogLevel::ERROR, "Cannot open file: " + path);
                return Result<std::string>(createFileError("Cannot open file", path));
            }
            
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            
            // Check file size limit (100MB)
            if (size > 100 * 1024 * 1024) {
                Logger::getInstance().log(LogLevel::ERROR, "File too large: " + path);
                return Result<std::string>(createFileError("File too large", path));
            }
            
            std::string content(size, '\0');
            file.read(&content[0], size);
            
            if (!file) {
                Logger::getInstance().log(LogLevel::ERROR, "Error reading file: " + path);
                return Result<std::string>(createFileError("Error reading file", path));
            }
            
            Logger::getInstance().log(LogLevel::INFO, "File loaded successfully: " + path);
            return Result<std::string>(std::move(content));
            
        } catch (const std::exception& e) {
            Logger::getInstance().log(LogLevel::ERROR, "Exception loading file " + path + ": " + e.what());
            return Result<std::string>(createFileError("Exception during file loading", path + " - " + e.what()));
        }
    }
    
    static Result<void> saveFile(const std::string& path, const std::string& content) {
        try {
            // Create directory if it doesn't exist
            std::filesystem::path filePath(path);
            std::filesystem::path dir = filePath.parent_path();
            
            if (!dir.empty() && !std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
            }
            
            std::ofstream file(path, std::ios::binary);
            if (!file) {
                Logger::getInstance().log(LogLevel::ERROR, "Cannot create file: " + path);
                return Result<void>(createFileError("Cannot create file", path));
            }
            
            file.write(content.c_str(), content.size());
            
            if (!file) {
                Logger::getInstance().log(LogLevel::ERROR, "Error writing file: " + path);
                return Result<void>(createFileError("Error writing file", path));
            }
            
            Logger::getInstance().log(LogLevel::INFO, "File saved successfully: " + path);
            return Result<void>();
            
        } catch (const std::exception& e) {
            Logger::getInstance().log(LogLevel::ERROR, "Exception saving file " + path + ": " + e.what());
            return Result<void>(createFileError("Exception during file saving", path + " - " + e.what()));
        }
    }
    
    static Result<void> copyFile(const std::string& source, const std::string& dest, bool overwrite = true) {
        try {
            std::filesystem::path src(source), dst(dest);
            // Validate source exists
            if (!std::filesystem::exists(src)) {
                return Result<void>(createFileError("Source file does not exist", source));
            }
            // Ensure dest directory exists
            auto parent = dst.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                std::filesystem::create_directories(parent);
            }
            auto opts = overwrite ? std::filesystem::copy_options::overwrite_existing
                                  : std::filesystem::copy_options::skip_existing;
            std::filesystem::copy_file(src, dst, opts);
            Logger::getInstance().log(LogLevel::INFO, "Copied file: " + source + " -> " + dest);
            return Result<void>();
        } catch (const std::exception& e) {
            return Result<void>(createFileError("Exception during copy", source + " -> " + dest + " - " + e.what()));
        }
    }
    
    static Result<void> copyFile(const std::wstring& source, const std::wstring& dest, bool overwrite = true) {
        try {
            std::filesystem::path src(source), dst(dest);
            // Validate source exists
            if (!std::filesystem::exists(src)) {
                std::string sourceStr(source.begin(), source.end());
                return Result<void>(createFileError("Source file does not exist", sourceStr));
            }
            // Ensure dest directory exists
            auto parent = dst.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent)) {
                std::filesystem::create_directories(parent);
            }
            auto opts = overwrite ? std::filesystem::copy_options::overwrite_existing
                                  : std::filesystem::copy_options::skip_existing;
            std::filesystem::copy_file(src, dst, opts);
            std::string sourceStr(source.begin(), source.end());
            std::string destStr(dest.begin(), dest.end());
            Logger::getInstance().log(LogLevel::INFO, "Copied file: " + sourceStr + " -> " + destStr);
            return Result<void>();
        } catch (const std::exception& e) {
            std::string sourceStr(source.begin(), source.end());
            std::string destStr(dest.begin(), dest.end());
            return Result<void>(createFileError("Exception during copy", sourceStr + " -> " + destStr + " - " + e.what()));
        }
    }
};

// RAII wrapper for Mix_Chunk
struct MixChunkDeleter {
    void operator()(Mix_Chunk* chunk) const {
        if (chunk) {
            Mix_FreeChunk(chunk);
        }
    }
};

using MixChunkPtr = std::unique_ptr<Mix_Chunk, MixChunkDeleter>;

// Safe Audio Operations (SDL_mixer integration)
class SafeAudioOperation {
public:
    static Result<MixChunkPtr> loadSound(const std::string& path) {
        auto pathResult = SafeFileOperation::validatePath(path);
        if (pathResult.isError()) {
            return Result<MixChunkPtr>(pathResult.getError());
        }
        
        std::string validatedPath = pathResult.getValue();
        
        if (!std::filesystem::exists(validatedPath)) {
            Logger::getInstance().log(LogLevel::ERROR, "Audio file does not exist: " + validatedPath);
            return Result<MixChunkPtr>(createAudioError("Audio file does not exist", validatedPath));
        }
        
        Mix_Chunk* rawChunk = Mix_LoadWAV(validatedPath.c_str());
        if (!rawChunk) {
            std::string mixError = Mix_GetError();
            Logger::getInstance().log(LogLevel::ERROR, "Failed to load audio: " + validatedPath + " - " + mixError);
            return Result<MixChunkPtr>(createAudioError("Failed to load audio", validatedPath + " - " + mixError));
        }
        
        MixChunkPtr chunk(rawChunk);
        Logger::getInstance().log(LogLevel::INFO, "Audio loaded successfully: " + validatedPath);
        return Result<MixChunkPtr>(std::move(chunk));
    }
    
    static Result<int> playSound(Mix_Chunk* chunk, int channel = -1) {
        if (!chunk) {
            Logger::getInstance().log(LogLevel::ERROR, "Null audio chunk provided");
            return Result<int>(createAudioError("Null audio chunk"));
        }
        
        int playedChannel = Mix_PlayChannel(channel, chunk, 0);
        if (playedChannel == -1) {
            std::string mixError = Mix_GetError();
            Logger::getInstance().log(LogLevel::ERROR, "Failed to play audio: " + mixError);
            return Result<int>(createAudioError("Failed to play audio", mixError));
        }
        
        Logger::getInstance().log(LogLevel::DEBUG, "Audio playing on channel: " + std::to_string(playedChannel));
        return Result<int>(playedChannel);
    }
    
    static Result<int> playSound(const MixChunkPtr& chunk, int channel = -1) {
        return playSound(chunk.get(), channel);
    }
};

// Convenience macros for integration
#define SAFE_CALL(operation) \
    do { \
        auto result = (operation); \
        if (result.isError()) { \
            Logger::getInstance().log(LogLevel::ERROR, \
                "Safe operation failed: " + result.getError().toString()); \
        } \
    } while(0)

#define LOG_RESULT_ERROR(result) \
    do { \
        if ((result).isError()) { \
            Logger::getInstance().log(LogLevel::ERROR, \
                "Operation failed: " + (result).getError().toString()); \
        } \
    } while(0)

#define SAFE_RETURN_ON_ERROR(result) \
    do { \
        if ((result).isError()) { \
            Logger::getInstance().log(LogLevel::ERROR, \
                "Early return due to error: " + (result).getError().toString()); \
            return; \
        } \
    } while(0)

#define SAFE_RETURN_VALUE_ON_ERROR(result, returnValue) \
    do { \
        if ((result).isError()) { \
            Logger::getInstance().log(LogLevel::ERROR, \
                "Early return due to error: " + (result).getError().toString()); \
            return (returnValue); \
        } \
    } while(0)

} // namespace Safe
} // namespace Core

#endif // SAFE_OPERATION_H