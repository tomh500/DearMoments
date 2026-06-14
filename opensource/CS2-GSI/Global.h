#pragma once
#define SDL_MAIN_HANDLED
#define NOMINMAX

#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <condition_variable>

#include <SDL.h>
#include <SDL_mixer.h>
#include "cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// --- 宏定义与常量 ---
constexpr int CH_KILL = 0;
constexpr int CH_MVP = 1;
constexpr int CH_WIN = 2;
constexpr int CH_BOMB = 3;

// --- 1. 游戏状态 (全部 extern) ---
extern std::string last_phase;
extern int last_kills;
extern bool dead_muted;
extern bool wait_for_kill_reset;
extern bool waiting_for_live;
extern bool round_started;
extern bool round_death_occurred;

// MVP 与 ACE 逻辑相关
extern bool mvp_flag;
extern bool mvp_played;
extern int mvp_counts;
extern int last_mvp_counts;
extern int mvp_candidate_kills;
extern int deathcounts_rd;
extern bool temp_disable_ace_when_mvp;
extern bool temp_to_disable_count_mvp;
extern bool match;
extern float vol;
extern bool debug_mode;
extern bool custom_musickit;
extern bool low_memory;
extern bool ShowMVP;
extern std::queue<int> kills_queue;
extern std::mutex queue_mutex;

// 基础配置与控制
extern bool print_raw_json;
extern std::wstring target_steamid;
extern std::atomic<bool> is_playing;
extern bool is_sdl_initialized;
extern std::atomic<bool> menu_active;
extern bool use_ogg;
extern bool show_source_data;
extern bool custom_musickit;
extern bool debug_mode;
extern bool custom_flashbang;
extern bool low_memory;
extern float vol; // 全局音量
extern bool low_memory;
extern bool custom_musickit;



// 炸弹状态
extern std::string last_bomb_state;
extern bool bomb_planted_this_round;
extern std::atomic<int> bomb_channel;
extern std::atomic<bool> bomb_sound_playing;

// 队列与并发
extern std::queue<int> kills_queue;
extern std::mutex queue_mutex;
extern std::mutex number_mutex;
extern std::mutex menu_mutex;

// --- 2. 资源映射 ---
extern std::unordered_map<int, Mix_Chunk*> sound_map;
extern std::unordered_map<int, std::string> sound_file_map;
extern bool ShowMVP;

// --- 3. 函数原型 ---
bool SetWorkingDirectory(const std::wstring& path);
void RunBatchFile(const std::wstring& exePath, const std::wstring& arguments, bool showWindow);
void ReceiveData(httplib::Server* svr, bool match, float vol, bool debug_mode);
void StartMenuLoop(float vol);
void StopMenuLoop();
void StopBombSound();
void gsi_playerflash();
void gsi_endplayerflash();
bool PreloadFlashImage(SDL_Renderer* renderer, const std::string& path);
void FreeFlashResources();
void BombChannelFinished(int channel);

// 音频初始化相关声明
bool LoadKillingSoundConfig(bool& match, float& vol, bool& ogg, bool& show_src);
void InitAudioOnce(bool ogg);
void PreloadSounds(bool ogg);
void BroadcastGSI(const std::string& json);

