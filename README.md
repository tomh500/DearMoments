# 🎮 Summer/Autumn 3.1

[![License](https://img.shields.io/badge/license-Personal%20Use-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)](https://github.com/neKamita/Summer)
[![CS2](https://img.shields.io/badge/game-CS2-orange.svg)](https://store.steampowered.com/app/730/CounterStrike_2/)
[![Version](https://img.shields.io/badge/version-3.1-green.svg)](https://github.com/neKamita/Summer/releases)

A comprehensive CS2 configuration management system with advanced GSI integration, modular architecture, and real-time game state processing.

## 📋 Table of Contents

- [🎮 Summer/Autumn 3.1](#-summerautumn-31)
  - [📋 Table of Contents](#-table-of-contents)
  - [🎯 Project Overview](#-project-overview)
  - [✨ Key Features](#-key-features)
    - [🎮 Core Systems](#-core-systems)
    - [🔧 Modules](#-modules)
    - [🎵 Audio Features](#-audio-features)
    - [🖥️ User Interface](#️-user-interface)
  - [🚀 Quick Start](#-quick-start)
  - [💾 Installation](#-installation)
    - [Windows Installation](#windows-installation)
    - [Linux Installation](#linux-installation)
  - [🎮 Usage](#-usage)
    - [GUI Configuration](#gui-configuration)
    - [Console Commands](#console-commands)
    - [Key Bindings](#key-bindings)
  - [📦 Module Documentation](#-module-documentation)
    - [Core Modules](#core-modules)
    - [Module Development](#module-development)
  - [🏗️ Architecture Overview](#️-architecture-overview)
    - [C++ Components](#c-components)
    - [GSI Server Architecture](#gsi-server-architecture)
    - [Data Flow](#data-flow)
  - [🛠️ Development Setup](#️-development-setup)
    - [Prerequisites](#prerequisites)
    - [Building](#building)
    - [Development Guidelines](#development-guidelines)
  - [📚 Configuration Reference](#-configuration-reference)
    - [Primary Files](#primary-files)
    - [Module Configurations](#module-configurations)
    - [GSI Configuration](#gsi-configuration)
  - [🌐 Multi-language Support](#-multi-language-support)
    - [Documentation Languages](#documentation-languages)
    - [Interface Languages](#interface-languages)
  - [❓ FAQ \& Troubleshooting](#-faq--troubleshooting)
    - [Common Issues](#common-issues)
    - [Uninstallation](#uninstallation)
  - [🤝 Contributing](#-contributing)
    - [Quick Contributing Steps](#quick-contributing-steps)
  - [📄 License \& Credits](#-license--credits)
    - [License](#license)
    - [Credits](#credits)
    - [Third-party Components](#third-party-components)
  - [🆘 Support \& Community](#-support--community)
    - [Getting Help](#getting-help)
    - [Reporting Bugs](#reporting-bugs)
    - [Feature Requests](#feature-requests)
    - [Development Contributions](#development-contributions)

## 🎯 Project Overview

Summer/Autumn 3.1 is an advanced CS2 configuration management system that enhances gameplay through intelligent automation and customization. Built with a modular C++ architecture, it provides:

- **Real-time Game State Integration (GSI)**: Advanced HTTP server processing CS2 game events
- **Modular System**: Plugin-based architecture for extensible functionality
- **Audio Enhancement**: Dynamic sound replacement and music kit systems
- **Cross-platform Support**: Native Windows and Linux compatibility
- **User-friendly Interface**: GUI-based configuration management
- **Performance Optimization**: High-performance C++ core with minimal overhead

## ✨ Key Features

### 🎮 Core Systems

- **GSI Server**: Real-time game state processing with JSON parsing
- **Module Loader**: Dynamic plugin system with IModule interface
- **Event System**: Publisher-subscriber pattern for component communication
- **Configuration Generator**: Automated CFG file generation and management
- **Audio Manager**: Advanced sound processing with SDL2 integration

### 🔧 Modules

- **AnlingMot**: Advanced movement assistance and optimization
- **Crosshair**: Dynamic crosshair customization and switching
- **MouseWheel**: Enhanced weapon switching and utility bindings
- **ThrowItems**: Intelligent grenade throw assistance
- **5eplay**: Competition platform integration
- **CFGLoader**: Advanced configuration loading and injection

### 🎵 Audio Features

- **Kill Sound Replacement**: Custom audio feedback for eliminations
- **Music Kit Integration**: Dynamic music system with game state awareness
- **Bomb Sound Enhancement**: Advanced audio cues for explosive scenarios
- **Buy Phase Audio**: Automated sound loops during purchase phases

### 🖥️ User Interface

- **RTSS Integration**: Real-time statistics and overlay support
- **GUI Configuration**: User-friendly settings management
- **Multi-language Support**: Chinese and English interfaces
- **Debug Mode**: Comprehensive logging and troubleshooting tools

## 🚀 Quick Start

1. **Download** the latest release from [Releases](https://github.com/neKamita/Summer/releases)
2. **Extract** to your CS2 cfg folder as `Autumn`
3. **Run** `CFG主程序.bat` (Windows) or `Linux_Installer` (Linux)
4. **Configure** through the GUI interface
5. **Launch CS2** with `+exec Autumn/setup` launch option

## 💾 Installation

### Windows Installation

1. **Prerequisites**:
   - CS2 installed and running
   - Administrator privileges for installation
   - Visual C++ Redistributable (included)

2. **Step-by-step**:

   ```bash
   # 1. Extract project to CS2 cfg directory
   # Path should be: steamapps/common/Counter-Strike Global Offensive/game/csgo/cfg/Autumn/
   
   # 2. Run the main installer
   CFG主程序.bat
   
   # 3. In the GUI:
   # - Click "文件" (File) → "更新并安装 CFG" (Update and Install CFG)
   # - Configure your settings in "用户空间" (User Space)
   # - Save configuration
   
   # 4. Add to CS2 launch options:
   +exec Autumn/setup
   ```

3. **Verification**:
   - Check console output for successful CFG loading
   - Verify GSI server starts on localhost:3000
   - Test module functionality in-game

### Linux Installation

1. **Prerequisites**:

   ```bash
   sudo apt-get update
   sudo apt-get install build-essential cmake libsdl2-dev libsdl2-mixer-dev
   ```

2. **Installation**:

   ```bash
   # 1. Extract to CS2 cfg directory
   # 2. Make installer executable
   chmod +x Linux_Installer
   
   # 3. Run installer
   ./Linux_Installer
   
   # 4. Follow GUI prompts similar to Windows
   ```

3. **Launch Options**:

   ```bash
   +exec Autumn/setup
   ```

## 🎮 Usage

### GUI Configuration

1. **Main Interface**:
   - Launch `CFG主程序.bat` or equivalent
   - Navigate through menu: File → Update and Install CFG
   - Access User Space for personalized settings

2. **Module Management**:
   - Enable/disable specific modules
   - Configure module-specific parameters
   - Generate custom configurations

3. **Audio Configuration**:
   - Set kill sound preferences in `setting/gsi.json`
   - Configure music kit options
   - Adjust volume levels and audio channels

### Console Commands

```bash
# Load configuration
exec Autumn/setup

# Debug mode toggle
bind "INSERT" "toggle developer 1"

# Reset bindings (uninstall)
binddefaults
```

### Key Bindings

- **INSERT**: Toggle debug overlay and text GUI
- **Custom binds**: Configured through module-specific settings

## 📦 Module Documentation

### Core Modules

| Module | Description | Configuration |
|--------|-------------|---------------|
| **AnlingMot** | Movement optimization and assistance | `code/Modules/AnlingMot/` |
| **Crosshair** | Dynamic crosshair management | `code/Modules/Crosshair/` |
| **MouseWheel** | Enhanced weapon switching | `code/Modules/MouseWheel/` |
| **ThrowItems** | Grenade throw assistance | `code/Modules/ThrowItems/` |
| **5eplay** | Competition platform integration | `code/Modules/5eplay/` |

### Module Development

Modules implement the `IModule` interface:

```cpp
class IModule {
public:
    virtual ~IModule() = default;
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void update() = 0;
};
```

## 🏗️ Architecture Overview

### C++ Components

```
code/CPP-OpenSource/
├── core/              # Core framework components
│   ├── ModuleLoader.h # Dynamic module loading
│   ├── EventSystem.h  # Event publishing system
│   ├── Logger.h       # Logging infrastructure
│   └── Constants.h    # System constants
├── gsi/               # Game State Integration
│   ├── AudioManager.h # Audio processing
│   ├── GameStateHandler.h # Game state logic
│   └── HttpServer.h   # HTTP server implementation
└── installer/         # Installation system
    └── [installation components]
```

### GSI Server Architecture

- **HTTP Server**: Built on cpp-httplib for high performance
- **JSON Processing**: SIMDJSON for optimized parsing
- **Event System**: Publisher-subscriber pattern for module communication
- **Audio Pipeline**: SDL2-based audio processing with channel management
- **Thread Safety**: Atomic operations and mutex protection

### Data Flow

```
CS2 Game → GSI → HTTP Server → JSON Parser → Event System → Modules → Audio/Config Output
```

## 🛠️ Development Setup

### Prerequisites

```bash
# Windows (Visual Studio)
- Visual Studio 2019 or later
- CMake 3.16+
- vcpkg for dependencies

# Linux
sudo apt-get install build-essential cmake git
sudo apt-get install libsdl2-dev libsdl2-mixer-dev
sudo apt-get install nlohmann-json3-dev libhttplib-dev
```

### Building

```bash
# Clone repository
git clone https://github.com/neKamita/Summer.git
cd Summer

# Build C++ components
cd code/CPP-OpenSource
mkdir build && cd build
cmake ..
make -j$(nproc)  # Linux
# or
cmake --build . --config Release  # Windows
```

### Development Guidelines

1. **Follow SOLID Principles**: Single responsibility, dependency injection
2. **Use Event System**: Publish events for inter-module communication
3. **Thread Safety**: Use atomic operations and proper synchronization
4. **Error Handling**: Comprehensive logging and graceful degradation
5. **Memory Management**: RAII and smart pointers throughout

## 📚 Configuration Reference

### Primary Files

- `Setup.cfg`: Main configuration entry point
- `setting/UserSetting.cfg`: User-specific settings
- `setting/UserKeyBinds.cfg`: Custom key bindings
- `setting/UserValue.cfg`: User preferences
- `setting/gsi.json`: GSI and audio configuration

### Module Configurations

Each module maintains its own configuration files in `code/Modules/[ModuleName]/`

### GSI Configuration

```json
{
  "uri": "http://localhost:3000",
  "timeout": 5.0,
  "buffer": 0.1,
  "throttle": 0.1,
  "heartbeat": 30.0,
  "data": {
    "provider": 1,
    "map": 1,
    "round": 1,
    "player_id": 1,
    "player_match_stats": 1,
    "player_state": 1,
    "player_weapons": 1,
    "allplayers": 1
  }
}
```

## 🌐 Multi-language Support

### Documentation Languages

- **Chinese (Primary)**: [`docs/主教程与更新日志.md`](docs/主教程与更新日志.md)
- **English**: [`docs/English_Version/Main & UpdateLog.md`](docs/English_Version/Main%20%26%20UpdateLog.md)

### Interface Languages

- GUI supports both Chinese and English
- Configuration files include multi-language comments
- Error messages and logs available in both languages

## ❓ FAQ & Troubleshooting

### Common Issues

**Q: Is this a cheat/hack?**
A: No. Summer/Autumn uses only official CS2 commands and configurations. It's a pure CFG project with no game process manipulation.

**Q: Why are there .exe files in the project?**
A: These are for installation, uninstallation, and verification purposes only. They don't interact with the CS2 game process and are safe.

**Q: Why are there .dll files?**
A: Used for module installation and system integration. They're unrelated to CS2 gameplay and pose no security risk.

**Q: Will this cause VAC bans?**
A: Summer/Autumn uses only official CS2 configurations. However, some community servers may have their own restrictions. Consult server administrators if concerned.

**Q: GSI server not starting?**
A:

- Check if port 3000 is available
- Verify CS2 GSI configuration
- Run with administrator privileges
- Check firewall settings

**Q: Audio not working?**
A:

- Verify SDL2 installation
- Check audio file paths in configuration
- Ensure proper file permissions
- Test with debug mode enabled

### Uninstallation

1. **GUI Method**: Use File → Uninstall CFG in main program
2. **Manual Method**:
   - Remove `exec Autumn/setup` from `autoexec.cfg`
   - Remove `+exec Autumn/setup` from launch options
   - Run `binddefaults` in console to reset bindings

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](docs/CONTRIBUTING.md) for detailed information on:

- Development environment setup
- Code style guidelines
- Module development
- Testing procedures
- Pull request process

### Quick Contributing Steps

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Commit changes: `git commit -m 'Add amazing feature'`
4. Push to branch: `git push origin feature/amazing-feature`
5. Open a Pull Request

## 📄 License & Credits

### License

- **Personal Use**: Free for individual use
- **Commercial Use**: Contact authors for licensing
- **Open Source**: Non-official versions must remain free and open source
- **Attribution**: Preserve copyright notices in derivatives

### Credits

- **Development Team**: Luotiany1_Mar and contributors
- **Libraries**: SDL2, nlohmann/json, cpp-httplib, SIMDJSON
- **Community**: Thanks to all testers and feedback providers

### Third-party Components

- [SDL2](https://www.libsdl.org/): Audio processing
- [nlohmann/json](https://github.com/nlohmann/json): JSON handling
- [cpp-httplib](https://github.com/yhirose/cpp-httplib): HTTP server
- [SIMDJSON](https://github.com/simdjson/simdjson): High-performance JSON parsing

## 🆘 Support & Community

### Getting Help

- **Issues**: [GitHub Issues](https://github.com/neKamita/Summer/issues)
- **Discussions**: [GitHub Discussions](https://github.com/neKamita/Summer/discussions)
- **Documentation**: Check `docs/` directory for detailed guides

### Reporting Bugs

When reporting bugs, please include:

- System information (OS, CS2 version, project version)
- Steps to reproduce the issue
- Expected vs actual behavior
- Configuration files and debug logs
- Screenshots or videos if applicable

### Feature Requests

- Use GitHub Issues with "enhancement" label
- Provide detailed use case descriptions
- Include implementation suggestions if possible

### Development Contributions

- Check [CONTRIBUTING.md](docs/CONTRIBUTING.md) for guidelines
- Look for "good first issue" labels for newcomers
- Join discussions on architecture and feature planning

---

**Last Updated**: September 2024 by Development Team
**Project Status**: Active Development
**Supported CS2 Versions**: Current and latest updates

For the most up-to-date information, please check our [GitHub repository](https://github.com/neKamita/Summer).