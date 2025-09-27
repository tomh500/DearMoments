# 🤝 Contributing to Summer/Autumn 3.1

Thank you for your interest in contributing to Summer/Autumn! This guide will help you understand our development process, coding standards, and how to contribute effectively.

## 📋 Table of Contents

- [Development Environment Setup](#-development-environment-setup)
- [Code Style Guidelines](#-code-style-guidelines)
- [Module Development](#-module-development)
- [Testing Guidelines](#-testing-guidelines)
- [Pull Request Process](#-pull-request-process)
- [Issue Reporting](#-issue-reporting)
- [Documentation Standards](#-documentation-standards)

## 🛠️ Development Environment Setup

### Prerequisites

#### Windows Development

```bash
# Install Visual Studio 2019 or later
# Install CMake 3.16 or later
# Install vcpkg for dependency management

# Clone vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install dependencies
.\vcpkg install sdl2 sdl2-mixer nlohmann-json cpp-httplib simdjson
```

#### Linux Development

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake git
sudo apt-get install libsdl2-dev libsdl2-mixer-dev
sudo apt-get install nlohmann-json3-dev cpp-httplib-dev
sudo apt-get install libsimdjson-dev

# Fedora/RHEL
sudo dnf install gcc-c++ cmake git
sudo dnf install SDL2-devel SDL2_mixer-devel
sudo dnf install json-devel cpp-httplib-devel
sudo dnf install simdjson-devel
```

### Repository Setup

```bash
# Fork the repository on GitHub
git clone https://github.com/YourUsername/Summer.git
cd Summer

# Add upstream remote
git remote add upstream https://github.com/neKamita/Summer.git

# Create development branch
git checkout -b feature/your-feature-name
```

### Building the Project

```bash
# Navigate to C++ source directory
cd code/CPP-OpenSource

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the project
# Linux
make -j$(nproc)

# Windows (Visual Studio)
cmake --build . --config Release
```

## 📝 Code Style Guidelines

### C++ Coding Standards

#### Naming Conventions

```cpp
// Classes: PascalCase
class ModuleLoader {
public:
    // Public methods: camelCase
    bool loadModule(const std::string& name);

    // Private members: m_ prefix + camelCase
private:
    std::string m_moduleName;
    static std::mutex s_instanceMutex;  // Static: s_ prefix
};

// Constants: UPPER_SNAKE_CASE
constexpr int MAX_MODULES = 100;

// Namespaces: PascalCase
namespace Core {
namespace Safe {
    // Nested namespaces allowed
}
}
```

#### Code Organization

```cpp
// Header file structure
#pragma once

// System includes first
#include <string>
#include <vector>
#include <memory>

// Third-party includes
#include <nlohmann/json.hpp>

// Project includes last
#include "Logger.h"
#include "SafeOperation.h"

namespace Core {
    class MyClass {
    public:
        // Public interface first
        explicit MyClass(const std::string& name);
        ~MyClass() = default;

        // Move/copy semantics
        MyClass(const MyClass&) = delete;
        MyClass& operator=(const MyClass&) = delete;

        // Public methods
        bool initialize();
        void shutdown();

    private:
        // Private members
        std::string m_name;
        std::unique_ptr<Implementation> m_impl;

        // Private methods
        void validateState() const;
    };
}
```

#### Error Handling

```cpp
// Use Safe::Result for error handling
Safe::Result<void> loadConfiguration() {
    try {
        // Implementation
        return Safe::Result<void>::success();
    } catch (const std::exception& e) {
        return Safe::Result<void>::failure(
            Safe::SafeError("CONFIG_LOAD_ERROR",
                           "Failed to load configuration: " + std::string(e.what()))
        );
    }
}

// Use RAII for resource management
class ResourceManager {
public:
    explicit ResourceManager(const std::string& resource)
        : m_resource(std::make_unique<Resource>(resource)) {
        // Acquire resource in constructor
    }

    ~ResourceManager() {
        // Release resource in destructor (automatic)
    }

private:
    std::unique_ptr<Resource> m_resource;
};
```

#### Thread Safety

```cpp
class ThreadSafeModule {
private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, Data> m_data;

public:
    // Read operations: shared_lock
    std::vector<std::string> getKeys() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        std::vector<std::string> keys;
        for (const auto& pair : m_data) {
            keys.push_back(pair.first);
        }
        return keys;
    }

    // Write operations: unique_lock
    void addData(const std::string& key, const Data& data) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_data[key] = data;
    }
};
```

### Configuration File Standards

#### CFG Files

```cfg
// Header comment with module information
// Module: ModuleName
// Version: 1.0.0
// Author: YourName
// Description: Brief module description

// Use descriptive aliases
alias module_enable "echo Module enabled"
alias module_disable "echo Module disabled"

// Group related functionality
// === MOVEMENT SETTINGS ===
bind "w" "+forward"
bind "a" "+moveleft"
bind "s" "+back"
bind "d" "+moveright"

// === WEAPON BINDINGS ===
bind "1" "slot1"
bind "2" "slot2"

// Use consistent formatting
setinfo module_version "1.0.0"
setinfo module_enabled "1"
```

## 🧩 Module Development

### IModule Interface Implementation

```cpp
#include "../modules/IModule.h"

class MyCustomModule : public Modules::Core::IModule {
public:
    explicit MyCustomModule(const std::string& configPath)
        : m_configPath(configPath), m_initialized(false) {}

    // Required interface methods
    Modules::Core::ModuleInfo getModuleInfo() const override {
        return {
            "MyCustomModule",           // name
            "1.0.0",                   // version
            "Custom functionality",     // description
            {"dependency1"},           // dependencies
            100,                       // priority (lower = loads first)
            true                       // enabled by default
        };
    }

    Safe::Result<void> initialize() override {
        if (m_initialized) {
            return Safe::Result<void>::success();
        }

        // Initialize module resources
        auto result = loadConfiguration();
        if (!result.isSuccess()) {
            return result;
        }

        m_initialized = true;
        LOG_INFO("MyCustomModule initialized successfully");
        return Safe::Result<void>::success();
    }

    Safe::Result<void> activate() override {
        if (!m_initialized) {
            return Safe::Result<void>::failure(
                Safe::SafeError("NOT_INITIALIZED", "Module not initialized")
            );
        }

        // Activate module functionality
        return Safe::Result<void>::success();
    }

    Safe::Result<void> deactivate() override {
        // Deactivate module functionality
        return Safe::Result<void>::success();
    }

    Safe::Result<void> cleanup() override {
        // Cleanup resources
        m_initialized = false;
        return Safe::Result<void>::success();
    }

    // Event handling
    std::vector<std::string> getEventSubscriptions() const override {
        return {"GamePhaseChanged", "PlayerKill", "BombPlanted"};
    }

    void handleEvent(const Core::Event& event) override {
        if (event.getType() == "GamePhaseChanged") {
            handleGamePhaseChanged(static_cast<const Core::GamePhaseChangedEvent&>(event));
        }
        // Handle other events...
    }

private:
    std::string m_configPath;
    bool m_initialized;

    Safe::Result<void> loadConfiguration() {
        // Load module-specific configuration
        return Safe::Result<void>::success();
    }

    void handleGamePhaseChanged(const Core::GamePhaseChangedEvent& event) {
        LOG_INFO_STREAM("Phase changed from " << event.getOldPhase()
                       << " to " << event.getNewPhase());
    }
};
```

### Module Directory Structure

```
code/Modules/YourModule/
├── _init_.cfg                 # Module entry point
├── config/
│   ├── settings.cfg          # Module settings
│   └── bindings.cfg          # Key bindings
├── scripts/
│   ├── main.cfg             # Main functionality
│   └── utils.cfg            # Utility functions
└── README.md                # Module documentation
```

### Module Configuration Template

```cfg
// _init_.cfg - Module Entry Point
// Module: YourModule
// Version: 1.0.0

// Load module configuration
exec YourModule/config/settings
exec YourModule/config/bindings

// Load module scripts
exec YourModule/scripts/main

// Module initialization
echo "YourModule v1.0.0 loaded successfully"
setinfo yourmodule_loaded "1"
```

## 🧪 Testing Guidelines

### Unit Testing

```cpp
// Use Google Test framework
#include <gtest/gtest.h>
#include "MyCustomModule.h"

class MyCustomModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        module = std::make_unique<MyCustomModule>("test_config.cfg");
    }

    void TearDown() override {
        if (module) {
            module->cleanup();
        }
    }

    std::unique_ptr<MyCustomModule> module;
};

TEST_F(MyCustomModuleTest, InitializationSuccess) {
    auto result = module->initialize();
    EXPECT_TRUE(result.isSuccess());

    auto info = module->getModuleInfo();
    EXPECT_EQ(info.name, "MyCustomModule");
    EXPECT_EQ(info.version, "1.0.0");
}

TEST_F(MyCustomModuleTest, ActivationRequiresInitialization) {
    auto result = module->activate();
    EXPECT_FALSE(result.isSuccess());
    EXPECT_EQ(result.getError().code, "NOT_INITIALIZED");
}
```

### Integration Testing

```cpp
// Test module interaction with core systems
class ModuleIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize core systems
        auto& eventSystem = Core::EventSystem::getInstance();
        auto& moduleLoader = Core::ModuleLoader::getInstance();

        // Load test modules
        moduleLoader.discoverModules("test/modules");
        moduleLoader.loadAllModules();
    }
};

TEST_F(ModuleIntegrationTest, EventPropagation) {
    // Test event handling between modules
    Core::GamePhaseChangedEvent event("buy", "live");
    Core::EventSystem::getInstance().publish(event);

    // Verify event was handled correctly
    // Add assertions based on expected behavior
}
```

### Manual Testing Checklist

- [ ] Module loads without errors
- [ ] Configuration files are properly parsed
- [ ] Key bindings work as expected
- [ ] Module responds to game state changes
- [ ] Performance impact is minimal
- [ ] Memory usage is reasonable
- [ ] Module unloads cleanly

## 🔄 Pull Request Process

### Before Submitting

1. **Code Quality Check**:

   ```bash
   # Run static analysis
   cppcheck --enable=all code/CPP-OpenSource/

   # Check code formatting
   clang-format -style=file -i **/*.cpp **/*.h

   # Run tests
   cd build && ctest --output-on-failure
   ```

2. **Documentation Update**:

   - Update relevant README sections
   - Add/update code comments
   - Update API documentation if needed

3. **Commit Message Format**:

   ```
   feat: add new crosshair customization module

   - Implements dynamic crosshair switching based on weapon
   - Adds configuration options for color and size
   - Integrates with existing weapon detection system

   Closes #123
   ```

### PR Template Checklist

- [ ] **Description**: Clear description of changes and motivation
- [ ] **Type**: Bug fix, feature, documentation, refactor, etc.
- [ ] **Testing**: How changes were tested
- [ ] **Breaking Changes**: Any breaking changes and migration notes
- [ ] **Documentation**: Updated documentation if needed
- [ ] **Performance**: Performance impact assessed
- [ ] **Security**: Security implications considered

### Review Process

1. **Automated Checks**: CI must pass
2. **Code Review**: At least one maintainer approval
3. **Testing**: Manual testing by reviewer
4. **Documentation**: Verify documentation is complete

## 🐛 Issue Reporting

### Bug Reports

Use the bug report template and include:

- **System Information**: OS, CS2 version, project version
- **Steps to Reproduce**: Detailed reproduction steps
- **Expected Behavior**: What should happen
- **Actual Behavior**: What actually happens
- **Logs**: Debug logs with timestamps
- **Configuration**: Relevant configuration files

### Feature Requests

Use the feature request template and include:

- **Feature Summary**: Brief description
- **Use Case**: Why this would be useful
- **Implementation Ideas**: Suggestions for implementation
- **Alternatives**: Other solutions considered

## 📚 Documentation Standards

### Code Documentation

```cpp
/**
 * @brief Loads and manages CS2 configuration modules
 *
 * The ModuleLoader provides a comprehensive system for discovering,
 * loading, and managing CS2 configuration modules. It supports
 * automatic discovery of modules in the filesystem and provides
 * thread-safe access to module instances.
 *
 * @example
 * auto& loader = ModuleLoader::getInstance();
 * auto result = loader.discoverModules("modules/");
 * if (result.isSuccess()) {
 *     loader.loadAllModules();
 * }
 */
class ModuleLoader {
public:
    /**
     * @brief Discovers modules in the specified directory
     * @param basePath The directory to scan for modules
     * @return Result indicating success or failure with error details
     */
    Safe::Result<void> discoverModules(const std::string& basePath);
};
```

### Markdown Documentation

- Use clear headings and structure
- Include code examples where helpful
- Add links to related documentation
- Keep language clear and concise
- Use emojis consistently for visual organization

### Configuration Documentation

```cfg
// === CROSSHAIR SETTINGS ===
// Crosshair color (0-5): 0=red, 1=green, 2=yellow, 3=blue, 4=cyan, 5=pink
cl_crosshaircolor "1"

// Crosshair size (1-10): Larger values = bigger crosshair
cl_crosshairsize "2"

// Dynamic crosshair (0/1): 0=static, 1=dynamic movement
cl_crosshairdynamic "0"
```

## 🎯 Development Best Practices

### SOLID Principles

- **Single Responsibility**: Each class has one reason to change
- **Open/Closed**: Open for extension, closed for modification
- **Liskov Substitution**: Derived classes must be substitutable
- **Interface Segregation**: Many specific interfaces vs one general
- **Dependency Inversion**: Depend on abstractions, not concretions

### Performance Guidelines

- Use `std::string_view` for read-only string parameters
- Prefer `emplace` over `push` for containers
- Use move semantics where appropriate
- Avoid unnecessary memory allocations in hot paths
- Profile performance-critical code sections

### Security Considerations

- Validate all input parameters
- Use bounds-checked operations
- Sanitize file paths and user input
- Follow principle of least privilege
- Log security-relevant events

## 🤔 Getting Help

- **Discord**: Join our development Discord (link in main README)
- **GitHub Discussions**: Use for questions and design discussions
- **Issues**: Create issues for bugs and feature requests
- **Documentation**: Check existing docs before asking questions

## 📄 License

By contributing to Summer/Autumn, you agree that your contributions will be licensed under the same terms as the project.

---

Thank you for contributing to Summer/Autumn! Your efforts help make CS2 configuration management better for everyone.