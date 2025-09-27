#pragma once

#include "InstallStrategy.h"
#include "../core/Logger.h"
#include "../core/SafeOperation.h"
#include <memory>
#include <string>
#include <filesystem>
#include <windows.h>
#include <process.h>

// Forward declaration of global function defined in CFGInstaller.cpp
bool AppendIfMissing(const std::wstring& filename, const std::wstring& content);

namespace Installer {

    /**
     * @brief Unified CFG installer that eliminates code duplication through strategy pattern
     * 
     * This class handles all installation logic while using language-specific strategies
     * for messages and configuration. Integrates with the existing core infrastructure
     * for safe operations, logging, and error handling.
     */
    class UnifiedCFGInstaller {
    private:
        std::unique_ptr<Strategy::IInstallStrategy> strategy_;
        
    public:
        /**
         * @brief Constructor accepting a language strategy
         * @param strategy Unique pointer to the appropriate strategy implementation
         */
        explicit UnifiedCFGInstaller(std::unique_ptr<Strategy::IInstallStrategy> strategy)
            : strategy_(std::move(strategy)) {
            if (!strategy_) {
                throw std::invalid_argument("Strategy cannot be null");
            }
        }

        /**
         * @brief Main installation method
         * @param hWnd Window handle for UI dialogs
         * @return 0 on success, -1 on error
         */
        int install(HWND hWnd) {
            try {
                LOG_INFO("Starting unified CFG installation");

                // Phase 1: Path Resolution and Validation
                if (!validatePaths()) {
                    showError(hWnd, Strategy::ErrorType::PATH_VALIDATION_FAILED);
                    return -1;
                }

                // Phase 2: Directory Creation
                if (!createDirectories(hWnd)) {
                    return -1;
                }

                // Phase 3: File Operations
                if (!copyFiles(hWnd)) {
                    return -1;
                }

                // Phase 4: User Confirmation for autoexec.cfg
                if (!handleAutoexecConfiguration(hWnd)) {
                    return -1;
                }

                // Phase 5: Service Startup
                if (!startServices(hWnd)) {
                    return -1;
                }

                // Phase 6: User Space Configuration
                handleUserSpaceConfiguration(hWnd);

                LOG_INFO("CFG installation completed successfully");
                return 0;

            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("Installation failed with exception: " << e.what());
                showError(hWnd, Strategy::ErrorType::PROCESS_START_FAILED);
                return -1;
            }
        }

    private:
        /**
         * @brief Validate all required paths exist
         * @return true if validation passes
         */
        bool validatePaths() {
            try {
                for (const auto& [source, _] : strategy_->getFileCopyOperations()) {
                    if (!std::filesystem::exists(std::filesystem::path(source))) {
                        LOG_ERROR_STREAM("Source missing: " << std::string(source.begin(), source.end()));
                        return false;
                    }
                }
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("Path validation exception: " << e.what());
                return false;
            }
        }

        /**
         * @brief Create required directories safely
         * @param hWnd Window handle for error dialogs
         * @return true if all directories created successfully
         */
        bool createDirectories(HWND hWnd) {
            try {
                auto directories = strategy_->getDirectoriesToCreate();
                for (const auto& dir : directories) {
                    std::filesystem::path dirPath(dir);
                    
                    if (!std::filesystem::exists(dirPath)) {
                        std::error_code ec;
                        if (!std::filesystem::create_directories(dirPath, ec)) {
                            LOG_ERROR_STREAM("Failed to create directory: " << dirPath.string() << " Error: " << ec.message());
                            
                            std::wstring errorMsg = strategy_->getErrorMessage(Strategy::ErrorType::DIRECTORY_CREATE_FAILED) + dir;
                            MessageBoxW(hWnd, errorMsg.c_str(), strategy_->getErrorTitle().c_str(), MB_OK | MB_ICONERROR);
                            return false;
                        }
                        
                        LOG_INFO_STREAM("Created directory: " << dirPath.string());
                    }
                }
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("Directory creation exception: " << e.what());
                showError(hWnd, Strategy::ErrorType::DIRECTORY_CREATE_FAILED);
                return false;
            }
        }

        /**
         * @brief Copy all required files safely
         * @param hWnd Window handle for error dialogs
         * @return true if all files copied successfully
         */
        bool copyFiles(HWND hWnd) {
            try {
                auto fileCopyOps = strategy_->getFileCopyOperations();
                for (const auto& [source, dest] : fileCopyOps) {
                    // Skip overwriting autoexec.cfg if it already exists
                    if (dest.size() >= 14 && dest.find(L"cfg\\autoexec.cfg") != std::wstring::npos) {
                        if (std::filesystem::exists(std::filesystem::path(dest))) {
                            LOG_INFO_STREAM("Skipping existing autoexec.cfg: " << std::string(dest.begin(), dest.end()));
                            continue;
                        }
                    }
                    
                    auto result = Core::Safe::SafeFileOperation::copyFile(source, dest, false);
                    if (!result.isSuccess()) {
                        std::string sourceLog(source.begin(), source.end());
                        std::string destLog(dest.begin(), dest.end());
                        LOG_ERROR_STREAM("File copy failed: " << sourceLog << " -> " << destLog << " Error: " << result.getError());
                        showError(hWnd, Strategy::ErrorType::FILE_COPY_FAILED);
                        return false;
                    }
                    
                    std::string sourceLog(source.begin(), source.end());
                    std::string destLog(dest.begin(), dest.end());
                    LOG_INFO_STREAM("Copied file: " << sourceLog << " -> " << destLog);
                }
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("File copy exception: " << e.what());
                showError(hWnd, Strategy::ErrorType::FILE_COPY_FAILED);
                return false;
            }
        }

        /**
         * @brief Handle autoexec.cfg configuration with user confirmation
         * @param hWnd Window handle for dialogs
         * @return true if handled successfully
         */
        bool handleAutoexecConfiguration(HWND hWnd) {
            try {
                // Show confirmation dialog
                std::wstring confirmMsg = strategy_->getConfirmationMessage(Strategy::ConfirmationType::ADD_TO_AUTOEXEC);
                int result = MessageBoxW(hWnd, confirmMsg.c_str(), strategy_->getConfirmationTitle().c_str(), MB_YESNO | MB_ICONQUESTION);
                
                if (result == IDYES) {
                    // Check if autoexec.cfg already contains the entry
                    if (::AppendIfMissing(L"cfg\\autoexec.cfg", L"exec Autumn/Setup")) {
                        std::wstring successMsg = strategy_->getAutoexecAddedMessage();
                        MessageBoxW(hWnd, successMsg.c_str(), strategy_->getSuccessTitle().c_str(), MB_OK | MB_ICONINFORMATION);
                        LOG_INFO("Added exec Autumn/Setup to autoexec.cfg");
                    } else {
                        std::wstring infoMsg = strategy_->getAutoexecExistsMessage();
                        MessageBoxW(hWnd, infoMsg.c_str(), strategy_->getInfoTitle().c_str(), MB_OK | MB_ICONINFORMATION);
                        LOG_INFO("autoexec.cfg already contains exec Autumn/Setup");
                    }
                }
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("Autoexec configuration exception: " << e.what());
                showError(hWnd, Strategy::ErrorType::AUTOEXEC_WRITE_FAILED);
                return false;
            }
        }

        /**
         * @brief Start required services and applications
         * @param hWnd Window handle for error dialogs
         * @return true if all services started successfully
         */
        bool startServices(HWND hWnd) {
            try {
                auto executables = strategy_->getExecutablesToStart();
                for (const auto& [exe, args] : executables) {
                    
                    if (!safeStartProcess(exe, args, false)) {
                        LOG_ERROR_STREAM("Failed to start process: " << std::string(exe.begin(), exe.end()));
                        
                        if (exe.find(L"LinkListener.exe") != std::wstring::npos) {
                            showError(hWnd, Strategy::ErrorType::LINKLISTENER_FAILED);
                        } else {
                            showError(hWnd, Strategy::ErrorType::PROCESS_START_FAILED);
                        }
                        return false;
                    }
                    
                    LOG_INFO_STREAM("Started process: " << std::string(exe.begin(), exe.end()));
                }
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("Service startup exception: " << e.what());
                showError(hWnd, Strategy::ErrorType::PROCESS_START_FAILED);
                return false;
            }
        }

        /**
         * @brief Handle user space configuration dialog
         * @param hWnd Window handle for dialogs
         */
        void handleUserSpaceConfiguration(HWND hWnd) {
            try {
                std::wstring confirmMsg = strategy_->getConfirmationMessage(Strategy::ConfirmationType::EDIT_USER_SPACE);
                int result = MessageBoxW(hWnd, confirmMsg.c_str(), strategy_->getConfirmationTitle().c_str(), MB_YESNO | MB_ICONQUESTION);
                
                if (result == IDYES) {
                    std::wstring asulPath = strategy_->getAsulLinkPath();
                    safeStartProcess(L"..\\extra\\application\\Asul_Editor.exe", asulPath, false);
                    LOG_INFO("Started Asul_Editor for user space configuration");
                }
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("User space configuration exception: " << e.what());
            }
        }

        /**
         * @brief Display error message using strategy
         * @param hWnd Window handle
         * @param errorType Type of error to display
         */
        void showError(HWND hWnd, Strategy::ErrorType errorType) {
            std::wstring errorMsg = strategy_->getErrorMessage(errorType);
            MessageBoxW(hWnd, errorMsg.c_str(), strategy_->getErrorTitle().c_str(), MB_OK | MB_ICONERROR);
        }

        /**
         * @brief Safe string to wstring conversion
         * @param str Input string
         * @return Wide string
         */
        std::wstring safeStringToWString(const std::string& str) {
            try {
                return std::wstring(str.begin(), str.end());
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("String conversion failed: " << e.what());
                return L"";
            }
        }

        /**
         * @brief Get debug mode setting (replaces missing global debug variable)
         * @return Debug mode flag
         */
        int getDebugMode() const {
            return strategy_->shouldShowDebugInfo() ? 1 : 0;
        }

        /**
         * @brief Safe process launching (replaces missing StartApps functions)
         * @param exe Executable path
         * @param args Command line arguments
         * @param wait Whether to wait for process completion
         * @return true if process started successfully
         */
        bool safeStartProcess(const std::wstring& exe, const std::wstring& args, bool wait) {
            try {
                STARTUPINFOW si = {};
                PROCESS_INFORMATION pi = {};
                si.cb = sizeof(si);
                
                std::wstring cmdLine = L"\"" + exe + L"\"";
                if (!args.empty()) {
                    cmdLine += L" \"" + args + L"\"";
                }
                
                // Create mutable copy for CreateProcessW
                std::vector<wchar_t> cmdLineMutable(cmdLine.begin(), cmdLine.end());
                cmdLineMutable.push_back(L'\0');
                
                BOOL result = CreateProcessW(
                    nullptr,                        // Application name
                    cmdLineMutable.data(),          // Command line
                    nullptr,                        // Process security attributes
                    nullptr,                        // Thread security attributes
                    FALSE,                          // Inherit handles
                    0,                              // Creation flags
                    nullptr,                        // Environment
                    nullptr,                        // Current directory
                    &si,                            // Startup info
                    &pi                             // Process information
                );
                
                if (result) {
                    if (wait) {
                        WaitForSingleObject(pi.hProcess, INFINITE);
                    }
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    return true;
                } else {
                    DWORD error = GetLastError();
                    LOG_ERROR_STREAM("CreateProcess failed with error: " << error);
                    return false;
                }
            } catch (const std::exception& e) {
                LOG_ERROR_STREAM("Process start exception: " << e.what());
                return false;
            }
        }

    };

} // namespace Installer