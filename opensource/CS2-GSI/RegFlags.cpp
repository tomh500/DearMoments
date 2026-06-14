#define SDL_MAIN_HANDLED
#include <ws2tcpip.h>
#include <iostream>
#include "cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>
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

#include "Global.h"
#include "NewGlobal.h"
namespace fs = std::filesystem;
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Shlwapi.lib")


std::wstring target_steamid = L"";
std::atomic<bool> is_playing(false);
bool wait_for_kill_reset = false;
bool round_death_occurred = false;
bool mvp_flag = false;
bool mvp_played = false;
int mvp_candidate_kills = 0;
bool temp_to_disable_count_mvp = false;
bool print_raw_json = false;


bool use_ogg = false;
bool show_source_data = false;
bool dead_muted = false;
int deathcounts_rd = 0;

int last_kills = -1;
bool round_started = false;
std::string last_phase = "";
bool waiting_for_live = false;
int mvp_counts = 0;
int last_mvp_counts = 0;
bool temp_disable_ace_when_mvp = false;
std::queue<int> kills_queue;
std::mutex queue_mutex;

static bool has_pushed_freeze = false;      // freeze(-7) 已推送
static bool has_pushed_roundstart = false;  // live(-8) 已推送

static std::atomic<bool> bomb_timer_active(false);
static std::thread    bomb_timer_thread;

static bool   player_died_this_round = false;
std::string last_bomb_state = "";
bool bomb_planted_this_round = false;
std::atomic<Mix_Chunk*> prev_number_chunk(nullptr);
std::atomic<Mix_Chunk*> prev_mvp_chunk(nullptr);

bool is_sdl_initialized = false;

std::atomic<bool> menu_active{ false };
std::atomic<bool> menu_loop_thread_running{ false };
std::thread menu_loop_thread;
bool low_memory = false;
bool custom_flashbang=false;