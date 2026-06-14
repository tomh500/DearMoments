#define SDL_MAIN_HANDLED
#include <ws2tcpip.h>
#include <algorithm>
#include <unordered_map>
#define NOMINMAX
#if defined(_WIN32) || defined(_WIN64)
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include<string.h>
#endif
#include "OverlayMVP.h"
#include "Global.h"
#include "NewGlobal.h"
#include "SteamHelper.h"
#include "SndTool.h"
namespace fs = std::filesystem;
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Shlwapi.lib")

std::mutex number_mutex;
std::mutex menu_mutex;

int last_kill_played = -1;  // 记录最后一次播放的击杀数，防止重复播放

bool match = true;
float vol = 0.8f;
bool debug_mode = true;
std::atomic<bool> bomb_sound_playing{ false };

extern void StartGSIBroadcast(); // 引用 GSIBroadcast.cpp 里的函数
//bool custom_musickit = true;
//std::atomic<int> bomb_channel{ -1 };

#pragma pack(push, 1)
struct SharedInfo {
    char version[32];
    char module_name[32];
    uint8_t is_active;   // 明确 1 字节
};
#pragma pack(pop)

/*void BombChannelFinished(int channel) {
    if (channel == bomb_channel.load()) {
        bomb_channel.store(-1);
    }
}*/

void StopBombSound() {
    int ch = bomb_channel.exchange(-1); // 原子操作确保只执行一次
    if (ch >= 0) {
        Mix_HaltChannel(ch);
        if (debug_mode) {
            std::cout << "[BOMB] Stopped bomb sound on channel " << ch << "\n";
        }
    }
}

void PreloadSounds(bool use_ogg) {
    if (low_memory) {
        std::cout << "[Preload] Skipped due to low_memory=true\n";
        return;  // 在低内存模式下跳过预加载
    }

    fs::path base_path = "setting/gsi/sounds";
    std::string ext = use_ogg ? ".ogg" : ".wav";

    // 根据 custom_musickit 决定预加载范围
    std::unordered_map<int, std::string> sounds;
    if (custom_musickit) {
        sounds = {
            { 1,   "1"       }, { 2,   "2"        }, { 3,   "3"       },
            { 4,   "4"       }, { 5,   "5"        }, {-1,  "extra"    },
            {-2,  "mvp"      }, {-3,  "win"      }, {-4,  "lose"     },
            {-12, "bomb"     }, {-13, "round"    }, {-14, "buy"      },
            {-18, "death"    }, {-19, "gameover"}, {-21, "menu"     }
        };
    }
    else {
        sounds = {
            { 1,  "1"    }, { 2,  "2"   }, { 3,  "3"    },
            { 4,  "4"    }, { 5,  "5"   }, {-1, "extra" }
        };
    }

    for (auto& [id, name] : sounds) {
        fs::path file = base_path / (name + ext);
        std::cout << "[Preload] Loading id=" << id
                  << " from: " << fs::absolute(file) << "\n";

        Mix_Chunk* chunk = Mix_LoadWAV(file.string().c_str());
        if (!chunk) {
            std::cerr << "[Preload] Failed to load " << file
                      << ": " << Mix_GetError() << "\n";
        }
        else {
            sound_map[id] = chunk;  // 在内存中缓存音效
        }
    }
}





void StartMenuLoop(float vol) {
   std:: cout << "1\n";

}

void StopMenuLoop() {
    std::lock_guard<std::mutex> lock(menu_mutex);
    menu_active = false;
    // 停止播放对应通道音效
    Mix_HaltChannel(3);  // 这里假设 -21 音效使用通道3播放，你播放时改用通道3
}

bool IsPortInUse(int port) {
    WSADATA wsaData;
    SOCKET sock = INVALID_SOCKET;
    sockaddr_in service;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return true; 
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return true;
    }

    service.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &service.sin_addr);
    service.sin_port = htons(port);

    // 如果绑定失败说明端口已被占用
    int result = bind(sock, (SOCKADDR*)&service, sizeof(service));

    closesocket(sock);
    WSACleanup();

    return result == SOCKET_ERROR;
}

void ReinitializeSDL() {
    if (Mix_Init(MIX_INIT_MP3) == 0) {
        std::cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << std::endl;
        return;
    }
    is_sdl_initialized = false; // 重新标记 SDL 未初始化
}


bool InitAudioSystem() {
    Mix_ChannelFinished(BombChannelFinished); // 注册回调
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init Audio failed: " << SDL_GetError() << std::endl;
        return false;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1) {
        std::cerr << "Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
        return false;
    }
    Mix_ReserveChannels(4);
    // 初始化支持ogg格式
    int mixFlags = MIX_INIT_OGG;
    if ((Mix_Init(mixFlags) & mixFlags) != mixFlags) {
        std::cerr << "Mix_Init OGG failed: " << Mix_GetError() << std::endl;
        return false;
    }
    return true;
}

void InitAudioOnce(bool use_ogg) {
    static bool done = false;
    if (done) return;
    done = true;

    SDL_Init(SDL_INIT_AUDIO);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    // 保留 0–3 ，自动分配（-1）只会用 >=4
    Mix_ReserveChannels(4);

    int mixFlags = MIX_INIT_OGG;
    if ((Mix_Init(mixFlags) & mixFlags) != mixFlags) {
        std::cerr << "Mix_Init OGG failed: " << Mix_GetError() << std::endl;
        return;
    }
   

    std::cout << "[Audio] Initialized. Channels 0–3 reserved.\n";
}




void PlaySoundFile_Interrupt(const fs::path& filepath, float vol, int channel, std::atomic<Mix_Chunk*>& prev_chunk_holder) {
    Mix_Chunk* chunk = Mix_LoadWAV(filepath.string().c_str());
    if (!chunk) {
        std::cerr << "Failed to load sound: " << filepath << ", Error: " << Mix_GetError() << std::endl;
        return;
    }
    int sdl_volume = static_cast<int>(vol * MIX_MAX_VOLUME);
    if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;
    Mix_VolumeChunk(chunk, sdl_volume);

    Mix_HaltChannel(channel);

    Mix_Chunk* old = prev_chunk_holder.exchange(chunk);
    if (old) Mix_FreeChunk(old);

    if (Mix_PlayChannel(channel, chunk, 0) == -1) {
        std::cerr << "Failed to play sound: " << filepath << ", Error: " << Mix_GetError() << std::endl;
        Mix_FreeChunk(chunk);
        prev_chunk_holder = nullptr;
    }
}

// 定义击杀播报的函数
void ReportKill(int kills_count) {
    switch (kills_count) {
    case 0: break;
    case 1:
        std::cout << "One kill!" << std::endl;
        break;
    case 2:
        std::cout << "Double kill!" << std::endl;
        break;
    case 3:
        std::cout << "Triple kill!" << std::endl;
        break;
    case 4:
        std::cout << "Quadra kill!" << std::endl;
        break;
    case 5:
        std::cout << "ACE!" << std::endl;
        break;
    default:
        std::cout << "Multiple kills!" << std::endl;
        break;
    }
}

// 线程安全队列

bool ShowMVP = false;
// 用于保存本地存储的击杀数，避免重复播放音效
int roundkill_local = -1;  // 初始化为一个不可能的数值
bool is_first_data_received = true;  // 标记是否是第一次接收到数据

// 从 killing_sound.json 文件读取配置


bool LoadKillingSoundConfig(bool& match, float& vol, bool& use_ogg, bool& show_source_data) {
    // 使用 std::filesystem 构造跨平台路径

	fs::path config_path = fs::current_path() / "setting" / "gsi.json";

    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cerr << "Failed to open " << config_path << " file!" << std::endl;
        return false;
    }

    json config;
    try {
        config_file >> config;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to parse " << config_path << ": " << e.what() << std::endl;
        return false;
    }

    if (config.contains("match") && config["match"].is_boolean()) {
        match = config["match"];
    }
    else {
        match = true;
    }

    if (config.contains("vol") && config["vol"].is_number()) {
        vol = config["vol"];
    }
    else {
        vol = 1.0f;
    }

    if (config.contains("ogg") && config["ogg"].is_boolean()) {
        use_ogg = config["ogg"];
    }
    else {
        use_ogg = false;
    }

    if (config.contains("SourceDataShow") && config["SourceDataShow"].is_boolean()) {
        show_source_data = config["SourceDataShow"];
    }
    else {
        show_source_data = false;
    }

    if (config.contains("custom_musickit") && config["custom_musickit"].is_boolean()) {
        custom_musickit = config["custom_musickit"];
    }
    else {
        custom_musickit = false;
    }

    if (config.contains("custom_flashbang") && config["custom_flashbang"].is_boolean()) {
      custom_flashbang = config["custom_flashbang"];
    }
    else {
       custom_flashbang = false;
    }

    if (config.contains("low_memory") && config["low_memory"].is_boolean()) {
        low_memory = config["low_memory"];  
    }
    else {
        low_memory = false;
    }

    if (config.contains("ShowMVP") && config["ShowMVP"].is_boolean()) {
        ShowMVP = config["ShowMVP"];
    }
    else {
        ShowMVP = false;
    }


    return true;
}

// 接收数据的线程
// 全局状态变量


void ProcessKillQueue(bool match, float vol, bool use_ogg) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!kills_queue.empty()) {
        int kills_count = kills_queue.front();
        kills_queue.pop();

        // 跳过重复的击杀数，防止连续多次播放同一音效
        if (kills_count == last_kill_played) {
            std::cout << "[ProcessKillQueue] Skipping duplicate kills_count=" << kills_count << std::endl;
            continue;
        }
        last_kill_played = kills_count;

        PlayKillSound(kills_count, match, vol, use_ogg);
    }
}

HANDLE hMapFile = NULL;
SharedInfo* pSharedData = nullptr;
int main(int argc, char* argv[]) {
    system("chcp 65001");
    SetWorkingDirectory(L"..\\..\\");

    //DLL
    fs::path exePath = fs::current_path();
    fs::path dllPath = exePath / "library" / "execute" / "dll";
    if (fs::exists(dllPath)) {
        // 4. 调用 Windows API 增加 DLL 搜索目录
        // 注意：SetDllDirectoryW 会覆盖默认路径，如果想保留原本的，可以使用 AddDllDirectory
        SetDllDirectoryW(dllPath.wstring().c_str());

        // 调试打印，看看路径对不对
         std::wcout << L"[SYSTEM] DLL 搜索路径已重定向至: " << dllPath.wstring() << std::endl;
    }



    // --- 新增：写入共享内存版本号 ---
    hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedInfo), "KICore_GSI_Ver");
    if (hMapFile) {
        pSharedData = (SharedInfo*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedInfo));
        if (pSharedData) {
            strcpy_s(pSharedData->version, "20260124");
            strcpy_s(pSharedData->module_name, "GSI_Broadcaster");
            pSharedData->is_active = true;
        }
    }
    // ----------------------------
    StartGSIBroadcast();
    if (IsPortInUse(1009)) {
        MessageBoxW(nullptr, L"端口 1009 已被占用，无法正常使用GSI服务", L"端口占用", MB_ICONERROR | MB_OK);
       
    }
    std::wcout << L"[DEBUG] Program started." << std::endl;
    httplib::Server svr;
    // 创建 SteamHelper 对象
    SteamHelper steamHelper;
    if (debug_mode)
    {
        std::cout << "[Debug] Loaded Steam User IDs:\n";
        for (const auto& id : steamHelper.GetSteamUserIDs()) {
            std::cout << "SteamID: " << id << std::endl;
        }
        std::cout << "[Debug] Converted Steam User IDs (64-bit):\n";
        for (const auto& id : steamHelper.GetSteamUserIDs()) {
            std::string steam64ID = steamHelper.ConvertToSteam64ID(id);  // 将 32 位 ID 转换为 64 位
            std::cout << "SteamID (64-bit): " << steam64ID << std::endl;
        }

    }

    // 如果 Steam 路径读取失败
    if (steamHelper.CallRegister2Steam() == L"Read Failed") {
        std::wcout << L"无法读取 Steam 安装路径!" << std::endl;
        return 1;
    }
    print_raw_json = false;
    debug_mode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
         if (arg == "-debug") {
			 debug_mode = true;
             print_raw_json = true;
             std::cout << "debug mode enabled: print raw json data" << std::endl;
        }
    }
   

    // 加载配置和音效
    if (!LoadKillingSoundConfig(match, vol, use_ogg, show_source_data)) {
        return -1;
    }
    InitAudioOnce(use_ogg);
    PreloadSounds(use_ogg);
    InitOverlayMVP();

    // 3. 启动接收线程
    std::thread receive_thread(ReceiveData, &svr, match, vol, debug_mode);

    // 4. 主循环 (这里我删掉了你代码里重复的一份循环)
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ProcessKillQueue(match, vol, use_ogg);

        // 如果这里是死循环，代码永远不会运行到下面的 join()
        // 控制台关闭时会直接跳到 ConsoleHandler
    }

    // 这里的代码正常情况下跑不到，除非 break 了循环
    if (receive_thread.joinable()) receive_thread.join();
    return 0;
}

#if defined(_WIN32) || defined(_WIN64)
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_CLOSE_EVENT || signal == CTRL_C_EVENT) {
        std::cout << "\n[SYSTEM] 正在释放资源并关闭共享内存..." << std::endl;

        // 1. 标记状态为下线
        if (pSharedData) {
            pSharedData->is_active = false;
            UnmapViewOfFile(pSharedData);
        }

        // 2. 关闭内核句柄
        if (hMapFile) {
            CloseHandle(hMapFile);
        }

        // 3. 原有的 SDL 清理
        SDL_Quit();

        // 给系统一点点时间处理
        Sleep(500);
        return TRUE;
    }
    return FALSE;
}
#endif