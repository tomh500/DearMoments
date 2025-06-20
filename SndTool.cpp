#include "Global.h"

constexpr int CH_KILL = 0;  // 击杀数字
constexpr int CH_MVP = 1;  // MVP
constexpr int CH_WIN = 2;  // WIN/LOSE
constexpr int CH_BOMB = 3;  // 炸弹安装
std::unordered_map<int, std::string> sound_file_map;
std::unordered_map<int, Mix_Chunk*> sound_map;

void PlaySoundFile(int id, float vol, int channel = -1) {
    // 低内存模式：每次播放时都从硬盘加载音效文件
    if (low_memory) {
        // 查找音效文件路径
        auto it = sound_file_map.find(id);
        if (it == sound_file_map.end()) {
            std::cerr << "No sound file mapped for id=" << id << std::endl;
            return;
        }

        std::string file_str = it->second;  // 获取文件路径
        Mix_Chunk* chunk = Mix_LoadWAV(file_str.c_str());

        if (!chunk) {
            std::cerr << "Failed to load sound: " << file_str << ", Error: " << Mix_GetError() << std::endl;
            return;
        }

        // 设置音量
        int sdl_volume = static_cast<int>(vol * MIX_MAX_VOLUME);
        if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;
        Mix_VolumeChunk(chunk, sdl_volume);

        // 若指定了播放通道，提前终止该通道上的旧音效
        if (channel >= 0) {
            Mix_HaltChannel(channel);
        }

        // 播放音效（0 次重复）
        int play_channel = Mix_PlayChannel(channel, chunk, 0);
        if (play_channel == -1) {
            std::cerr << "Failed to play sound: " << file_str << ", Error: " << Mix_GetError() << std::endl;
            Mix_FreeChunk(chunk);  // 使用完后立刻释放
            return;
        }

        // 播放完后释放音效文件
        std::thread([chunk, play_channel]() {
            while (Mix_Playing(play_channel)) {
                SDL_Delay(6);  // 等待较短时间
            }
            Mix_FreeChunk(chunk);  // 释放音效文件
            }).detach();
        return;
    }

    // 非低内存模式：使用预加载的音效
    if (sound_map.find(id) != sound_map.end()) {
        Mix_Chunk* chunk = sound_map[id];

        // 设置音量
        int sdl_volume = static_cast<int>(vol * MIX_MAX_VOLUME);
        if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;
        Mix_VolumeChunk(chunk, sdl_volume);

        // 若指定了播放通道，提前终止该通道上的旧音效
        if (channel >= 0) {
            Mix_HaltChannel(channel);
        }

        // 播放音效（0 次重复）
        int play_channel = Mix_PlayChannel(channel, chunk, 0);
        if (play_channel == -1) {
            std::cerr << "Failed to play sound: " << Mix_GetError() << std::endl;
            return;
        }

        return;
    }

    // 如果音效没有被预加载，重新加载并播放
    auto it = sound_file_map.find(id);
    if (it == sound_file_map.end()) {
        std::cerr << "No sound file mapped for id=" << id << std::endl;
        return;
    }

    std::string file_str = it->second;  // 获取文件路径
    Mix_Chunk* chunk = Mix_LoadWAV(file_str.c_str());
    if (!chunk) {
        std::cerr << "Failed to load sound: " << file_str << ", Error: " << Mix_GetError() << std::endl;
        return;
    }

    // 设置音量
    int sdl_volume = static_cast<int>(vol * MIX_MAX_VOLUME);
    if (sdl_volume > MIX_MAX_VOLUME) sdl_volume = MIX_MAX_VOLUME;
    Mix_VolumeChunk(chunk, sdl_volume);

    // 播放音效
    int play_channel = Mix_PlayChannel(channel, chunk, 0);
    if (play_channel == -1) {
        std::cerr << "Failed to play sound: " << file_str << ", Error: " << Mix_GetError() << std::endl;
        Mix_FreeChunk(chunk);
        return;
    }

    // 将音效缓存到内存中以便后续播放
    sound_map[id] = chunk;
}

void PlayKillSound(int id, bool match, float vol, bool use_ogg) {
    if (!match) return;

    // 如果 low_memory 模式开启，使用磁盘临时播放方式
    if (low_memory) {
        // 根据 custom_musickit 选择不同的 name_map
        std::unordered_map<int, std::string> name_map;
        if (custom_musickit) {
            // 自定义音乐包：排除 1-5 和 extra（假设 extra 的 id 是 -1）
            name_map = {
                {-3, "win"},   {-4, "lose"}, {-2, "mvp"},
                {-12, "bomb"}, {-13, "round"},{-14, "buy"},
                {-18, "death"},{-19, "gameover"},{-21, "menu"},
                { 1, "1" }, {2, "2"}, {3, "3"}, {4, "4"}, {5, "5"},
                {-1, "extra"}
            };
        }
        else {
            // 默认模式：仅加载 1-5 和 extra(-1)
            name_map = {
                {1, "1"}, {2, "2"}, {3, "3"}, {4, "4"}, {5, "5"},
                {-1, "extra"}
            };
        }

        // 根据 id 查找对应的音效文件名
        auto it = name_map.find(id);
        if (it == name_map.end()) {
            std::cerr << "[PlayKillSound] low_memory: No file name for id=" << id
                << " (custom_musickit=" << custom_musickit << ")\n";
            return;
        }

        // 构造完整的文件路径
        std::string file_str = "Userspace/CS2/gsi/sounds/" + it->second + (use_ogg ? ".ogg" : ".wav");

        // 存入映射表以便之后播放
        sound_file_map[id] = file_str;

        // 选择对应的播放通道
        int channel = -1;
        switch (id) {
        case -12: channel = CH_BOMB;      bomb_channel = CH_BOMB; break;
        case -2:  channel = CH_MVP;       StopBombSound();        break;
        case -3:
        case -4:  channel = CH_WIN;       StopBombSound();        break;
        case 1: case 2: case 3: case 4: case 5: channel = CH_KILL; break;
        case -1: channel = CH_KILL;      break;
        default:  channel = -1;           break;
        }

        std::cout << "[Debug] [low_memory] PlaySoundFile for id=" << id
            << ", file=" << file_str << "\n";

        // 播放音效文件
        PlaySoundFile(id, vol, channel);
        return;
    }

    // ===== 正常预加载模式 =====
    auto it = sound_map.find(id);
    if (it == sound_map.end()) {
        std::cerr << "[PlayKillSound] Sound not found for id=" << id << std::endl;
        return;
    }

    Mix_Chunk* chunk = it->second;
    Mix_VolumeChunk(chunk, std::clamp(int(vol * MIX_MAX_VOLUME), 0, MIX_MAX_VOLUME));

    std::cout << "[Debug] PlayKillSound called id=" << id << "\n";
    switch (id) {
    case -12:
        std::cout << "[Debug] → case -12: play bomb on CH_BOMB=" << CH_BOMB << "\n";
        Mix_HaltChannel(CH_BOMB);
        bomb_channel = CH_BOMB;
        Mix_PlayChannel(CH_BOMB, chunk, -1);
        break;
    case -2:
        std::cout << "[Debug] → case -2: MVP\n";
        StopBombSound();
        Mix_HaltChannel(CH_MVP);
        Mix_PlayChannel(CH_MVP, chunk, 0);
        break;
    case -3: case -4:
        std::cout << "[Debug] → case " << id << ": WIN/LOSE\n";
        StopBombSound();
        Mix_HaltChannel(CH_WIN);
        Mix_PlayChannel(CH_WIN, chunk, 0);
        break;
    case 1: case 2: case 3: case 4: case 5:
        std::cout << "[Debug] → case " << id << ": KILL\n";
        Mix_HaltChannel(CH_KILL);
        Mix_PlayChannel(CH_KILL, chunk, 0);
        break;
    default:
        std::cout << "[Debug] → default: auto channel\n";
        Mix_PlayChannel(-1, chunk, 0);
        break;
    }
}