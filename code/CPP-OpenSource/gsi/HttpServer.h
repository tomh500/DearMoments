#pragma once

#include <string>
#include <thread>
#include <iostream>
#include <algorithm>
#include "cpp-httplib/httplib.h"
#include <simdjson.h>

#include "../core/SafeOperation.h"
#include "../core/InputValidator.h"
#include "../core/Logger.h"
#include "../core/Constants.h"

/**
 * @brief Comprehensive HTTP server management system for GSI
 * 
 * Extracts all HTTP handling from GSI.cpp:
 * - Server configuration and worker thread setup
 * - Request validation pipeline with InputValidator
 * - GSI data validation with warning-level logging
 * - JSON processing pipeline with SIMD acceleration
 * - Request filtering and performance optimization
 * - Response management and error handling
 * 
 * Preserves all existing functionality and integration patterns.
 */
class HttpServer {
private:
    HttpServer() = default;

public:
    /**
     * @brief Meyers singleton pattern for server management - thread-safe since C++11
     */
    static HttpServer& getInstance() {
        static HttpServer instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    /**
     * @brief Configure HTTP server with performance optimizations
     * 
     * Extract from ReceiveData server configuration:
     * - TCP nodelay for performance
     * - Dynamic worker thread pool based on hardware concurrency
     * - Custom task queue implementation
     * 
     * @param svr HTTP server instance to configure
     */
    inline void configureServer(httplib::Server* svr) {
        // ================== 网络层优化 ==================
        svr->set_tcp_nodelay(true);
        
        // Установка количества рабочих потоков с учетом констант
        const size_t workers = std::max<size_t>(
            static_cast<size_t>(Constants::Network::HTTP_WORKER_THREADS), 
            std::thread::hardware_concurrency() * 2
        );
        
        svr->new_task_queue = [workers]() -> httplib::TaskQueue* {
            return new httplib::ThreadPool(workers);
        };
    }

    /**
     * @brief Validate HTTP request using InputValidator
     * 
     * Extract from ReceiveData HTTP validation:
     * - Request size limits and content-type validation
     * - Early return with appropriate HTTP status codes
     * - Comprehensive error logging with validation details
     * 
     * @param req HTTP request to validate
     * @return ValidationResult with detailed feedback
     */
    inline Core::Validation::ValidationError validateRequest(const httplib::Request& req) {
        return Core::Validation::InputValidator::validateHTTPRequest(req);
    }

    /**
     * @brief Validate GSI-specific data using InputValidator
     * 
     * Extract from ReceiveData GSI validation:
     * - Schema validation for CS2 game state integration data
     * - Warning-level logging for validation failures (non-breaking)
     * - Compatibility mode for continued processing
     * 
     * @param requestBody JSON request body to validate
     * @return ValidationResult with GSI-specific checks
     */
    inline Core::Validation::ValidationError validateGSIData(const std::string& requestBody) {
        if (!requestBody.empty()) {
            return Core::Validation::InputValidator::validateGSIData(requestBody);
        }
        return Core::Validation::ValidationError(Core::Validation::ValidationResult::SUCCESS, "Empty body, skipping GSI validation");
    }

    /**
     * @brief Process JSON request with SIMD acceleration
     * 
     * Extract from ReceiveData JSON processing:
     * - Fast phase extraction using FastExtractPhase
     * - Safe JSON parsing with SIMD acceleration
     * - Comprehensive error handling and logging
     * 
     * @param requestBody JSON request body to process
     * @return SafeResult containing parsed JSON element or error
     */
    inline Core::Safe::Result<simdjson::dom::element> processJSONRequest(const std::string& requestBody) {
        // ================== 安全JSON解析 ==================
        return Core::Safe::SafeJSONOperation::parse(requestBody);
    }

    /**
     * @brief Fast phase extraction for request filtering
     * 
     * Extract from ReceiveData fast phase logic:
     * - Hot-path optimization for common request patterns
     * - Early filtering of invalid phases
     * - Performance optimization for valid requests
     * 
     * @param phase Extracted phase from request
     * @return true if request should be processed, false to skip
     */
    inline bool shouldProcessRequest(const std::string& phase) {
        // 阶段快速过滤 - 跳过无效请求
        return (phase == "live" || phase == "over" || phase == "gameover");
    }

    /**
     * @brief Send standardized HTTP response
     * 
     * Extract response handling patterns:
     * - Content-type management for different response types
     * - Status code handling for various error conditions
     * - Response content formatting
     * 
     * @param res HTTP response object
     * @param content Response content
     * @param contentType MIME type for response
     * @param statusCode HTTP status code (default: 200)
     */
    inline void sendResponse(httplib::Response& res, const std::string& content, 
                            const std::string& contentType = "text/plain", int statusCode = 200) {
        res.status = statusCode;
        res.set_content(content, contentType);
    }

    /**
     * @brief Handle HTTP validation errors with appropriate responses
     * 
     * Extract from ReceiveData validation error handling:
     * - Map validation results to HTTP status codes
     * - Provide appropriate error messages
     * - Log validation failures with context
     * 
     * @param validationResult Result from request validation
     * @param res HTTP response object to populate
     * @param debug_mode Whether debug mode is enabled
     * @return true if error was handled, false to continue processing
     */
    inline bool handleValidationError(const Core::Validation::ValidationError& validationResult, 
                                     httplib::Response& res, bool debug_mode) {
        if (validationResult.result != Core::Validation::ValidationResult::SUCCESS) {
            Logger::getInstance().log(LogLevel::WARNING, "HTTP validation failed: " + validationResult.message);
            if (debug_mode) {
                std::cout << "[HTTP VALIDATION] Failed: " << validationResult.message << "\n";
            }
            
            // Early return с соответствующим статус кодом
            if (validationResult.result == Core::Validation::ValidationResult::INVALID_REQUEST) {
                sendResponse(res, "Request Too Large", "text/plain", 413);  // Payload Too Large
            } else if (validationResult.result == Core::Validation::ValidationResult::INVALID_FORMAT) {
                sendResponse(res, "Unsupported Media Type", "text/plain", 415);  // Unsupported Media Type
            } else {
                sendResponse(res, "Bad Request", "text/plain", 400);  // Bad Request
            }
            return true;  // Error handled
        }
        return false;  // Continue processing
    }

    /**
     * @brief Handle GSI validation warnings
     * 
     * Extract from ReceiveData GSI validation handling:
     * - Log GSI validation warnings
     * - Continue processing for compatibility
     * - Provide debug output for validation issues
     * 
     * @param validationResult Result from GSI validation
     * @param debug_mode Whether debug mode is enabled
     */
    inline void handleGSIValidationWarning(const Core::Validation::ValidationError& validationResult, 
                                          bool debug_mode) {
        if (validationResult.result != Core::Validation::ValidationResult::SUCCESS) {
            Logger::getInstance().log(LogLevel::WARNING, "GSI data validation failed: " + validationResult.message);
            if (debug_mode) {
                std::cout << "[GSI VALIDATION] Warning: " << validationResult.message << "\n";
            }
            // Продолжаем обработку для совместимости
        }
    }

    /**
     * @brief Handle JSON parsing errors
     * 
     * Extract from ReceiveData JSON error handling:
     * - Log JSON parsing failures with context
     * - Provide appropriate error responses
     * - Debug output for parsing issues
     * 
     * @param jsonResult Result from JSON parsing operation
     * @param res HTTP response object
     * @param debug_mode Whether debug mode is enabled
     * @return true if error was handled, false to continue processing
     */
    inline bool handleJSONError(const Core::Safe::Result<simdjson::dom::element>& jsonResult, 
                               httplib::Response& res, bool debug_mode) {
        if (!jsonResult.isSuccess()) {
            LOG_RESULT_ERROR(jsonResult);
            if (debug_mode) {
                std::cerr << "[JSON ERROR] Safe parse failed: " << jsonResult.getError().toString() << std::endl;
            }
            sendResponse(res, "JSON PARSE ERROR", "text/plain", 400);
            return true;  // Error handled
        }
        return false;  // Continue processing
    }

    /**
     * @brief Log raw JSON for debugging purposes
     * 
     * Extract from ReceiveData debug logging:
     * - Raw JSON logging with proper formatting
     * - Debug mode conditional output
     * - Performance timing integration
     * 
     * @param requestBody JSON request body
     * @param debug_mode Whether debug mode is enabled
     */
    inline void logRawJSON(const std::string& requestBody, bool debug_mode) {
        if (debug_mode) {
            std::cout << "[RAW JSON RECEIVED]:\n" << requestBody << "\n";
        }
    }

    /**
     * @brief Fast phase extraction utility - inline implementation
     * 
     * Extract FastExtractPhase function:
     * - Optimized phase extraction for hot-path filtering
     * - String parsing without full JSON decode
     * - Performance-critical path optimization
     * 
     * @param requestBody JSON request body
     * @return Extracted phase string
     */
    inline std::string fastExtractPhase(const std::string& requestBody) {
        constexpr std::string_view phase_key = "\"phase\"";
        
        size_t pos = requestBody.find(phase_key);
        if (pos == std::string::npos) {
            return "";
        }
        
        // Move to end of key
        pos += phase_key.length();
        
        // Find colon after key
        pos = requestBody.find(':', pos);
        if (pos == std::string::npos) {
            return "";
        }
        
        pos++; // Skip colon
        
        // Skip whitespace characters
        while (pos < requestBody.size() && 
               (requestBody[pos] == ' ' || requestBody[pos] == '\t' || 
                requestBody[pos] == '\n' || requestBody[pos] == '\r')) {
            pos++;
        }
        
        // Expect string starting with quote
        if (pos >= requestBody.size() || requestBody[pos] != '"') {
            return "";
        }
        
        pos++; // Skip opening quote
        size_t start = pos;
        size_t end = requestBody.find('"', start);
        
        if (end == std::string::npos || end - start > 20) {
            return "";
        }
        
        return requestBody.substr(start, end - start);
    }

    /**
     * @brief Handle phase filtering with debug output
     * 
     * Extract from ReceiveData phase filtering:
     * - Fast-path filtering for invalid phases
     * - Debug logging for filtered requests
     * - Input sanitization for safe logging
     * 
     * @param phase Extracted phase string
     * @param res HTTP response object
     * @param debug_mode Whether debug mode is enabled
     * @return true if request was filtered, false to continue processing
     */
    inline bool handlePhaseFilter(const std::string& phase, httplib::Response& res, bool debug_mode) {
        if (!shouldProcessRequest(phase)) {
            if (debug_mode) {
                auto phase_safe = Core::Validation::InputValidator::sanitizeString(phase.empty() ? "<empty>" : phase);
                std::cout << "[FAST SKIP] Invalid phase: " << phase_safe << "\n";
            }
            sendResponse(res, "IGNORED", "text/plain", 200);
            return true;  // Request filtered
        }
        return false;  // Continue processing
    }

    /**
     * @brief Complete HTTP request processing pipeline
     * 
     * Orchestrates the full HTTP request handling:
     * - Validation pipeline
     * - JSON processing
     * - Phase filtering
     * - Error handling
     * 
     * @param req HTTP request
     * @param res HTTP response
     * @param debug_mode Debug mode flag
     * @param callback Function to call with parsed JSON for game state processing
     * @return true if processing completed, false if early return occurred
     */
    template<typename Callback>
    inline bool processRequest(const httplib::Request& req, httplib::Response& res, 
                              bool debug_mode, Callback callback) {
        // HTTP request validation
        auto validationResult = validateRequest(req);
        if (handleValidationError(validationResult, res, debug_mode)) {
            return false;
        }

        // Fast phase extraction and filtering
        std::string phase = fastExtractPhase(req.body);
        if (handlePhaseFilter(phase, res, debug_mode)) {
            return false;
        }

        // GSI data validation (warning-level)
        auto gsiValidation = validateGSIData(req.body);
        handleGSIValidationWarning(gsiValidation, debug_mode);

        // Debug logging
        logRawJSON(req.body, debug_mode);

        // JSON parsing
        auto jsonResult = processJSONRequest(req.body);
        if (handleJSONError(jsonResult, res, debug_mode)) {
            return false;
        }

        // Call the game state processing callback
        callback(jsonResult.getValue());
        
        return true;
    }
};