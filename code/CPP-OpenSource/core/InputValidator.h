#ifndef INPUT_VALIDATOR_H
#define INPUT_VALIDATOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <regex>
#include <algorithm>
#include <sstream>
#include "core/Logger.h"

namespace Core {
namespace Validation {

enum class ConfigType {
    GSI_JSON,
    RTSS_YAML,
    GENERAL_JSON,
    GENERAL_YAML
};

enum class ValidationResult {
    SUCCESS,
    INVALID_FORMAT,
    MISSING_REQUIRED_FIELD,
    INVALID_TYPE,
    OUT_OF_RANGE,
    INVALID_REQUEST,
    SCHEMA_VIOLATION
};

struct ValidationError {
    ValidationResult result;
    std::string message;
    std::string field;
    std::string context;
    
    ValidationError(ValidationResult r, const std::string& msg, const std::string& f = "", const std::string& ctx = "")
        : result(r), message(msg), field(f), context(ctx) {}
};

class InputValidator {
private:
    static constexpr size_t MAX_REQUEST_SIZE = 10 * 1024 * 1024; // 10MB limit
    static constexpr size_t MAX_JSON_DEPTH = 32;
    
    // GSI Schema validation helpers
    static bool validateGSIPlayer(const std::string& playerSection);
    static bool validateGSIRound(const std::string& roundSection);
    static bool validateGSIMap(const std::string& mapSection);
    static bool validateGSIProvider(const std::string& providerSection);
    
    // YAML validation helpers
    static bool validateRTSSKeyBindings(const std::string& content);
    static bool validateRTSSSettings(const std::string& content);
    
    // HTTP validation helpers
    static bool isValidContentType(const std::string& contentType);
    static bool isValidJSONStructure(const std::string& json);
    
    // Common validation utilities
    static bool isValidKeyName(const std::string& keyName);
    static bool isValidFPSRange(int fps);
    static std::string extractJSONField(const std::string& json, const std::string& field);
    
public:
    // GSI Data Validation
    static ValidationError validateGSIData(const std::string& json);
    
    // Config File Validation
    static ValidationError validateConfigFile(const std::string& content, ConfigType type);
    
    // HTTP Request Validation
    template<typename RequestType>
    static ValidationError validateHTTPRequest(const RequestType& req);
    
    // String Sanitization
    static std::string sanitizeString(const std::string& input);
    
    // Numeric Range Validation
    template<typename T>
    static bool validateNumericRange(T value, T min, T max) {
        return value >= min && value <= max;
    }
    
    // Specialized validators
    static ValidationError validatePlayerHealth(int health);
    static ValidationError validateVolumeLevel(float volume);
    static ValidationError validateSteamID(const std::string& steamId);
    static ValidationError validateFilePath(const std::string& path);
    
    // Utility functions
    static bool isValidJSON(const std::string& json);
    static bool isValidYAML(const std::string& yaml);
    static std::vector<std::string> getRequiredGSIFields();
    static std::vector<std::string> getRequiredRTSSFields();
};

// Template implementation for HTTP request validation
template<typename RequestType>
ValidationError InputValidator::validateHTTPRequest(const RequestType& req) {
    // Check request size
    if (req.body.size() > MAX_REQUEST_SIZE) {
        Logger::getInstance().log(LogLevel::WARNING, 
            "HTTP request too large: " + std::to_string(req.body.size()) + " bytes");
        return ValidationError(ValidationResult::INVALID_REQUEST, 
            "Request size exceeds limit", "body", "size: " + std::to_string(req.body.size()));
    }
    
    // Validate content type
    auto contentTypeIt = req.headers.find("Content-Type");
    if (contentTypeIt != req.headers.end()) {
        if (!isValidContentType(contentTypeIt->second)) {
            Logger::getInstance().log(LogLevel::WARNING, 
                "Invalid content type: " + contentTypeIt->second);
            return ValidationError(ValidationResult::INVALID_REQUEST, 
                "Invalid content type", "Content-Type", contentTypeIt->second);
        }
    }
    
    // Validate JSON structure if body is not empty
    if (!req.body.empty()) {
        if (!isValidJSONStructure(req.body)) {
            Logger::getInstance().log(LogLevel::WARNING, "Invalid JSON structure in request body");
            return ValidationError(ValidationResult::INVALID_FORMAT, 
                "Invalid JSON structure", "body", "malformed JSON");
        }
    }
    
    return ValidationError(ValidationResult::SUCCESS, "Request valid");
}

// GSI Data Validation Implementation
inline ValidationError InputValidator::validateGSIData(const std::string& json) {
    if (json.empty()) {
        Logger::getInstance().log(LogLevel::ERROR, "Empty GSI data received");
        return ValidationError(ValidationResult::INVALID_FORMAT, "Empty GSI data");
    }
    
    if (!isValidJSON(json)) {
        Logger::getInstance().log(LogLevel::ERROR, "Invalid JSON format in GSI data");
        return ValidationError(ValidationResult::INVALID_FORMAT, "Invalid JSON format");
    }
    
    // Check required fields
    std::vector<std::string> requiredFields = {"player", "round", "map", "provider"};
    for (const auto& field : requiredFields) {
        std::string fieldValue = extractJSONField(json, field);
        if (fieldValue.empty()) {
            Logger::getInstance().log(LogLevel::ERROR, "Missing required GSI field: " + field);
            return ValidationError(ValidationResult::MISSING_REQUIRED_FIELD, 
                "Missing required field", field, "GSI data");
        }
    }
    
    // Validate specific sections
    if (!validateGSIPlayer(extractJSONField(json, "player"))) {
        return ValidationError(ValidationResult::SCHEMA_VIOLATION, 
            "Invalid player data", "player", "GSI validation");
    }
    
    if (!validateGSIRound(extractJSONField(json, "round"))) {
        return ValidationError(ValidationResult::SCHEMA_VIOLATION, 
            "Invalid round data", "round", "GSI validation");
    }
    
    if (!validateGSIMap(extractJSONField(json, "map"))) {
        return ValidationError(ValidationResult::SCHEMA_VIOLATION, 
            "Invalid map data", "map", "GSI validation");
    }
    
    if (!validateGSIProvider(extractJSONField(json, "provider"))) {
        return ValidationError(ValidationResult::SCHEMA_VIOLATION, 
            "Invalid provider data", "provider", "GSI validation");
    }
    
    return ValidationError(ValidationResult::SUCCESS, "GSI data valid");
}

// Config File Validation Implementation
inline ValidationError InputValidator::validateConfigFile(const std::string& content, ConfigType type) {
    if (content.empty()) {
        Logger::getInstance().log(LogLevel::ERROR, "Empty config file content");
        return ValidationError(ValidationResult::INVALID_FORMAT, "Empty config content");
    }
    
    switch (type) {
        case ConfigType::GSI_JSON:
            return validateGSIData(content);
            
        case ConfigType::RTSS_YAML:
            if (!isValidYAML(content)) {
                Logger::getInstance().log(LogLevel::ERROR, "Invalid YAML format in RTSS config");
                return ValidationError(ValidationResult::INVALID_FORMAT, "Invalid YAML format");
            }
            if (!validateRTSSKeyBindings(content)) {
                return ValidationError(ValidationResult::SCHEMA_VIOLATION, 
                    "Invalid key bindings", "keybindings", "RTSS config");
            }
            break;
            
        case ConfigType::GENERAL_JSON:
            if (!isValidJSON(content)) {
                Logger::getInstance().log(LogLevel::ERROR, "Invalid JSON format in config");
                return ValidationError(ValidationResult::INVALID_FORMAT, "Invalid JSON format");
            }
            break;
            
        case ConfigType::GENERAL_YAML:
            if (!isValidYAML(content)) {
                Logger::getInstance().log(LogLevel::ERROR, "Invalid YAML format in config");
                return ValidationError(ValidationResult::INVALID_FORMAT, "Invalid YAML format");
            }
            break;
    }
    
    return ValidationError(ValidationResult::SUCCESS, "Config file valid");
}

// String Sanitization Implementation
inline std::string InputValidator::sanitizeString(const std::string& input) {
    std::string sanitized = input;
    
    // Remove potential XSS patterns
    std::regex scriptPattern(R"(<script[^>]*>.*?</script>)", std::regex_constants::icase);
    sanitized = std::regex_replace(sanitized, scriptPattern, "");
    
    // Remove potential SQL injection patterns
    std::regex sqlPattern(R"(\b(union|select|insert|update|delete|drop|create|alter)\b)", 
                         std::regex_constants::icase);
    sanitized = std::regex_replace(sanitized, sqlPattern, "");
    
    // Remove null bytes and control characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), 
        [](char c) { return c >= 0 && c < 32 && c != '\t' && c != '\n' && c != '\r'; }), 
        sanitized.end());
    
    // Limit length
    if (sanitized.length() > 1024) {
        sanitized = sanitized.substr(0, 1024);
        Logger::getInstance().log(LogLevel::WARNING, "String truncated during sanitization");
    }
    
    return sanitized;
}

// Specialized Validators Implementation
inline ValidationError InputValidator::validatePlayerHealth(int health) {
    if (!validateNumericRange(health, 0, 100)) {
        Logger::getInstance().log(LogLevel::WARNING, 
            "Invalid player health value: " + std::to_string(health));
        return ValidationError(ValidationResult::OUT_OF_RANGE, 
            "Health must be 0-100", "health", std::to_string(health));
    }
    return ValidationError(ValidationResult::SUCCESS, "Health valid");
}

inline ValidationError InputValidator::validateVolumeLevel(float volume) {
    if (!validateNumericRange(volume, 0.0f, 1.0f)) {
        Logger::getInstance().log(LogLevel::WARNING, 
            "Invalid volume level: " + std::to_string(volume));
        return ValidationError(ValidationResult::OUT_OF_RANGE, 
            "Volume must be 0.0-1.0", "volume", std::to_string(volume));
    }
    return ValidationError(ValidationResult::SUCCESS, "Volume valid");
}

inline ValidationError InputValidator::validateSteamID(const std::string& steamId) {
    if (steamId.empty()) {
        return ValidationError(ValidationResult::INVALID_FORMAT, "Empty Steam ID");
    }
    
    // Basic Steam ID format validation (64-bit integer)
    std::regex steamIdPattern(R"(^\d{17}$)");
    if (!std::regex_match(steamId, steamIdPattern)) {
        Logger::getInstance().log(LogLevel::WARNING, "Invalid Steam ID format: " + steamId);
        return ValidationError(ValidationResult::INVALID_FORMAT, 
            "Invalid Steam ID format", "steamid", steamId);
    }
    
    return ValidationError(ValidationResult::SUCCESS, "Steam ID valid");
}

inline ValidationError InputValidator::validateFilePath(const std::string& path) {
    if (path.empty()) {
        return ValidationError(ValidationResult::INVALID_FORMAT, "Empty file path");
    }
    
    // Check for directory traversal attempts
    if (path.find("..") != std::string::npos) {
        Logger::getInstance().log(LogLevel::WARNING, 
            "Directory traversal attempt in path: " + path);
        return ValidationError(ValidationResult::INVALID_REQUEST, 
            "Directory traversal not allowed", "path", path);
    }
    
    // Check for invalid characters
    std::regex invalidChars(R"([<>:"|?*])");
    if (std::regex_search(path, invalidChars)) {
        Logger::getInstance().log(LogLevel::WARNING, 
            "Invalid characters in file path: " + path);
        return ValidationError(ValidationResult::INVALID_FORMAT, 
            "Invalid characters in path", "path", path);
    }
    
    return ValidationError(ValidationResult::SUCCESS, "File path valid");
}

// Utility Functions Implementation
inline bool InputValidator::isValidJSON(const std::string& json) {
    // Basic JSON structure validation
    if (json.empty()) return false;
    
    int braceCount = 0;
    int bracketCount = 0;
    bool inString = false;
    bool escaped = false;
    
    for (char c : json) {
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (c == '\\' && inString) {
            escaped = true;
            continue;
        }
        
        if (c == '"') {
            inString = !inString;
            continue;
        }
        
        if (!inString) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
            else if (c == '[') bracketCount++;
            else if (c == ']') bracketCount--;
        }
    }
    
    return braceCount == 0 && bracketCount == 0 && !inString;
}

inline bool InputValidator::isValidYAML(const std::string& yaml) {
    // Basic YAML structure validation
    if (yaml.empty()) return false;
    
    // Check for basic YAML syntax issues
    std::vector<std::string> lines;
    std::stringstream ss(yaml);
    std::string line;
    
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    
    for (const auto& line : lines) {
        // Skip empty lines and comments
        std::string trimmed = line;
        trimmed.erase(0, trimmed.find_first_not_of(" \t"));
        if (trimmed.empty() || trimmed[0] == '#') continue;
        
        // Check for key-value pairs
        if (trimmed.find(':') == std::string::npos) {
            // Must be a list item or continuation
            if (trimmed[0] != '-' && trimmed[0] != ' ') {
                return false;
            }
        }
    }
    
    return true;
}

// Helper function implementations would go here...
inline bool InputValidator::validateGSIPlayer(const std::string& playerSection) {
    // Implementation for player validation
    return !playerSection.empty();
}

inline bool InputValidator::validateGSIRound(const std::string& roundSection) {
    // Implementation for round validation
    return !roundSection.empty();
}

inline bool InputValidator::validateGSIMap(const std::string& mapSection) {
    // Implementation for map validation
    return !mapSection.empty();
}

inline bool InputValidator::validateGSIProvider(const std::string& providerSection) {
    // Implementation for provider validation
    return !providerSection.empty();
}

inline bool InputValidator::validateRTSSKeyBindings(const std::string& content) {
    // Implementation for RTSS key binding validation
    return content.find("keybindings") != std::string::npos;
}

inline bool InputValidator::validateRTSSSettings(const std::string& content) {
    // Implementation for RTSS settings validation
    return !content.empty();
}

inline bool InputValidator::isValidContentType(const std::string& contentType) {
    return contentType.find("application/json") != std::string::npos ||
           contentType.find("text/plain") != std::string::npos;
}

inline bool InputValidator::isValidJSONStructure(const std::string& json) {
    return isValidJSON(json);
}

inline bool InputValidator::isValidKeyName(const std::string& keyName) {
    // List of valid key names for RTSS
    static const std::vector<std::string> validKeys = {
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", 
        "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
        "CTRL", "ALT", "SHIFT", "TAB", "ENTER", "SPACE"
    };
    
    return std::find(validKeys.begin(), validKeys.end(), keyName) != validKeys.end();
}

inline bool InputValidator::isValidFPSRange(int fps) {
    return fps >= 30 && fps <= 240;
}

inline std::string InputValidator::extractJSONField(const std::string& json, const std::string& field) {
    // Simple JSON field extraction
    std::string pattern = "\"" + field + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    
    // Skip whitespace
    pos++;
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    
    if (pos >= json.length()) return "";
    
    // Extract value
    if (json[pos] == '"') {
        // String value
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        // Numeric or boolean value
        size_t end = json.find_first_of(",}]", pos);
        if (end == std::string::npos) end = json.length();
        return json.substr(pos, end - pos);
    }
}

inline std::vector<std::string> InputValidator::getRequiredGSIFields() {
    return {"player", "round", "map", "provider"};
}

inline std::vector<std::string> InputValidator::getRequiredRTSSFields() {
    return {"keybindings", "ReadConsole"};
}

} // namespace Validation
} // namespace Core

#endif // INPUT_VALIDATOR_H
