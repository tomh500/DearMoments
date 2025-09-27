#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>

namespace Constants {

// Network Constants
namespace Network {
    constexpr const char* GSI_SERVER_HOST = "127.0.0.1";
    constexpr int GSI_SERVER_PORT = 1009;
    constexpr int HTTP_WORKER_THREADS = 4;
}

// Path Constants
namespace Paths {
    constexpr const char* USERSPACE_BASE_PATH = "Userspace";
    constexpr const char* GSI_SOUND_PATH = "Userspace/gsi/killing_sound";
    constexpr const char* CONFIG_DIR = "setting";
    constexpr const char* EXTRA_DIR = "extra";
    constexpr const char* RESOURCES_DIR = "extra/resources";
    constexpr const char* LOG_DIR = "logs";
    constexpr const char* AUTUMN_BASE_PATH = "code";
    constexpr const char* MODULES_DIR = "code/Modules";
    constexpr const char* TOOLS_DIR = "code/Tools";
}

// Audio Constants
namespace Audio {
    constexpr float DEFAULT_VOLUME = 1.0f;
    constexpr int BUY_CHANNEL = 2;
    constexpr int MAX_AUDIO_CHANNELS = 32;
}

// File Extensions
namespace FileExtensions {
    constexpr const char* AUDIO_EXT_WAV = ".wav";
    constexpr const char* AUDIO_EXT_OGG = ".ogg";
    constexpr const char* CONFIG_EXT = ".cfg";
    constexpr const char* YAML_EXT = ".yml";
    constexpr const char* JSON_EXT = ".json";
}

// Configuration File Names
namespace ConfigFiles {
    constexpr const char* USER_SETTING = "setting/UserSetting.cfg";
    constexpr const char* USER_KEYBINDS = "setting/UserKeyBinds.cfg";
    constexpr const char* USER_VALUE = "setting/UserValue.cfg";
    constexpr const char* RTSS_MORTIS = "setting/RTSS_mortis.yml";
    constexpr const char* GSI_CONFIG = "setting/gsi.json";
}

// Game State Integration Constants
namespace GSI {
    constexpr const char* GAMESTATE_INTEGRATION_FILE = "gamestate_integration_autumn.cfg";
    constexpr const char* FASTCONFIG_FILE = "fastconfig.asulink";
    constexpr const char* FASTCONFIG_EN_FILE = "fastconfig_EN.asulink";
}

// Application Constants
namespace Application {
    constexpr const char* APP_NAME = "Summer Controller";
    constexpr const char* APP_VERSION = "1.0.0";
    constexpr const char* LOG_FILE_PREFIX = "summer_";
    constexpr int MAX_LOG_FILE_SIZE = 10 * 1024 * 1024; // 10MB
}

// CS2 Game Constants
namespace CS2 {
    constexpr const char* ROUND_PHASE_FREEZETIME = "freezetime";
    constexpr const char* ROUND_PHASE_LIVE = "live";
    constexpr const char* ROUND_PHASE_OVER = "over";
    constexpr const char* PLAYER_ACTIVITY_PLAYING = "playing";
    constexpr const char* PLAYER_ACTIVITY_MENU = "menu";
    constexpr const char* BOMB_STATE_PLANTED = "planted";
    constexpr const char* BOMB_STATE_DEFUSED = "defused";
    constexpr const char* BOMB_STATE_EXPLODED = "exploded";
}

// Debug and Performance Constants
namespace Debug {
    constexpr bool DEFAULT_DEBUG_MODE = false;
    constexpr int PERFORMANCE_LOG_INTERVAL_MS = 1000;
    constexpr int MAX_JSON_PARSE_ERRORS = 10;
}

// HTTP Server Constants
namespace HTTP {
    constexpr int DEFAULT_TIMEOUT_MS = 5000;
    constexpr int MAX_REQUEST_SIZE = 1024 * 1024; // 1MB
    constexpr const char* CONTENT_TYPE_JSON = "application/json";
    constexpr const char* CONTENT_TYPE_TEXT = "text/plain";
}

} // namespace Constants

#endif // CONSTANTS_H