#pragma once
#define SDL_MAIN_HANDLED
#include <ws2tcpip.h>
#include <iostream>
#include "cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <SDL.h>
#include <SDL_mixer.h>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <queue>
#include <mutex>
#include <atomic>   
#include <shlwapi.h>
#include <unordered_map>
#include <chrono>
#include <fstream>
#define NOMINMAX
#include <filesystem>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif


extern std::string last_phase;
extern int last_kills;
extern bool dead_muted;
extern bool wait_for_kill_reset;
extern bool waiting_for_live;
extern bool round_started;

extern bool mvp_flag;
extern bool mvp_flag;
extern bool mvp_played;
extern int mvp_candidate_kills;
extern int mvp_candidate_kills;
extern bool round_death_occurred;
extern int mvp_counts;
extern int last_mvp_counts;
extern bool temp_disable_ace_when_mvp;
extern int deathcounts_rd ;
extern bool temp_to_disable_count_mvp;

using json = nlohmann::json;

extern std::wstring target_steamid;

extern std::atomic<bool> is_playing;
extern bool is_sdl_initialized;

extern bool wait_for_kill_reset;
extern bool round_death_occurred;
extern bool mvp_flag;
extern bool mvp_played;
extern int mvp_candidate_kills;
extern std::atomic<bool> menu_active;

extern bool use_ogg;
extern bool show_source_data;

extern std::queue<int> kills_queue;
extern std::mutex queue_mutex;
extern bool custom_musickit;
extern bool debug_mode;

extern std::string last_bomb_state;
extern bool bomb_planted_this_round;
extern std::atomic<int> bomb_channel;
extern bool custom_flashbang;
extern bool low_memory;

bool SetWorkingDirectory(const std::wstring& path);
void RunBatchFile(const std::wstring& exePath, const std::wstring& arguments, bool showWindow);
void ReceiveData(httplib::Server* svr, bool match, float vol, bool debug_mode);
void StartMenuLoop(float vol);
void StopMenuLoop();


void StopBombSound();
void  gsi_playerflash();
void  gsi_endplayerflash();
bool PreloadFlashImage(SDL_Renderer* renderer, const std::string& path);

void FreeFlashResources();
extern std::unordered_map<int, Mix_Chunk*> sound_map;
extern std::unordered_map<int, std::string> sound_file_map;
