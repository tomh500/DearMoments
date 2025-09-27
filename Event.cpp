#define SDL_MAIN_HANDLED
#include <ws2tcpip.h>
#include <iostream>
#include "cpp-httplib/httplib.h"
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <SDL_mixer.h>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <queue>
#include <mutex>
#include <shlwapi.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#endif

#include "OverlayMVP.h"
#include <simdjson.h> // 添加 SIMDJSON 库

// 热点字段快速提取函数
// 热点字段快速提取函数 - 更健壮的版本，允许键和值之间有空格

std::string FastExtractPhase(const std::string& body) {

    constexpr std::string_view phase_key = "\"phase\"";

    size_t pos = body.find(phase_key);

    if (pos == std::string::npos) {

        return "";

    }

    // 移动到键的末尾

    pos += phase_key.length();

    // 在键后找到冒号

    pos = body.find(':', pos);

    if (pos == std::string::npos) {

        return "";

    }

    pos++; // 跳过冒号

    // 跳过任何空白字符（空格、制表符、换行符等）

    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t' || body[pos] == '\n' || body[pos] == '\r')) {

        pos++;

    }

    // 现在期望一个以双引号开头的字符串

    if (pos >= body.size() || body[pos] != '"') {

        return "";

    }

    pos++; // 跳过开头的引号

    size_t start = pos;

    size_t end = body.find('"', start);

    if (end == std::string::npos || end - start > 20) {

        return "";

    }

    return body.substr(start, end - start);

}
#include "Global.h"
#include "SteamHelper.h"
using namespace std;
namespace fs = filesystem;

bool custom_musickit = false;
static std::string player_team;               // "CT" or "T"

// 每回合开始时两队的比分快照
static int round_score_ct = 0;
static int round_score_t = 0;

bool gameover_pushed = false;


// 用于追踪数字音效是否正在播放的标志，线程安全
static std::atomic<bool> is_number_sound_playing{ false };

// 全局变量，用于保存炸弹音效通道
std::atomic<int> bomb_channel{ -1 };

// 在全局区域添加：
void BombChannelFinished(int channel) {
    if (channel == bomb_channel) {
        bomb_channel = -1;
        bomb_sound_playing = false;
        if (debug_mode) {
            std::cout << "[BOMB] Bomb sound finished on channel " << channel << "\n";
        }
    }
}


// 播放炸弹音效时，记录通道
void PlayBombSound(const fs::path& sound_file, float vol) {
    // 确保只有一个炸弹音效在播放
    StopBombSound();

    Mix_Chunk* chunk = Mix_LoadWAV(sound_file.string().c_str());
    if (!chunk) {
        std::cerr << "Failed to load bomb sound: " << Mix_GetError() << std::endl;
        return;
    }

    int channel = Mix_PlayChannel(-1, chunk, 0);
    if (channel == -1) {
        std::cerr << "Failed to play bomb sound: " << Mix_GetError() << std::endl;
        Mix_FreeChunk(chunk);
        return;
    }

    bomb_channel = channel;
    Mix_Volume(channel, static_cast<int>(MIX_MAX_VOLUME * vol));
    bomb_sound_playing = true;

    if (debug_mode) {
        std::cout << "[BOMB] Playing bomb sound on channel " << channel << "\n";
    }

    // 设置回调在音效结束时自动清理
    // 注意：需要实现BombChannelFinished函数
}

// 停止所有声音
void StopAllSounds() {
    Mix_HaltChannel(-1);
    bomb_channel = -1;
}

std::atomic<bool> number_playing = false;
std::atomic<bool> is_buying(false);
std::thread buy_thread;
std::mutex buy_mutex;
std::condition_variable buy_cv;
int buy_channel = 2;  // 用于播放buy音效的指定通道

void BuySoundLoop(float vol) {
    fs::path base_path = "Userspace/gsi/killing_sound";
    std::string ext = use_ogg ? ".ogg" : ".wav";
    fs::path buy_sound = base_path / ("buy" + ext);

    while (true) {
        {
            std::unique_lock<std::mutex> lk(buy_mutex);
            if (!is_buying) break;
        }

        // 如果buy音效没有在播放，则播放
        if (!Mix_Playing(buy_channel)) {
            Mix_Chunk* chunk = Mix_LoadWAV(buy_sound.string().c_str());
            if (!chunk) {
                std::cerr << "Failed to load buy sound: " << Mix_GetError() << std::endl;
                break;
            }
            int sdl_volume = static_cast<int>(vol * MIX_MAX_VOLUME);
            sdl_volume = (sdl_volume > MIX_MAX_VOLUME) ? MIX_MAX_VOLUME : sdl_volume;
            Mix_VolumeChunk(chunk, sdl_volume);
            Mix_PlayChannel(buy_channel, chunk, 0);

            // 等待音效播放完毕再释放
            while (Mix_Playing(buy_channel)) {
                SDL_Delay(50);
            }
            Mix_FreeChunk(chunk);
        }
        else {
            // buy音效还在播放，等一会儿再检测
            SDL_Delay(50);
        }
    }
}

void OnLivePhaseStart(float vol) {
    // 停止所有音效
    Mix_HaltChannel(-1);

    // 如果之前处于buy循环，结束buy循环线程
    {
        std::lock_guard<std::mutex> lk(buy_mutex);
        is_buying = false;
    }
    if (buy_thread.joinable()) {
        buy_thread.join();
    }

    // 标记非buy状态
    is_buying = false;
}

void OnBuyPhaseEnd() {
    {
        std::lock_guard<std::mutex> lk(buy_mutex);
        is_buying = false;
    }
    if (buy_thread.joinable()) {
        buy_thread.join();
    }
}

void OnBuyPhaseStart(float vol) {
    {
        std::lock_guard<std::mutex> lk(buy_mutex);
        if (is_buying) {
            // 已经在播放buy音效，不重复推送
            return;
        }
        is_buying = true;
    }

    // 启动buy音效循环线程
    buy_thread = std::thread(BuySoundLoop, vol);
}



int steamid_in_list = 0;
void ReceiveData(httplib::Server* svr, bool match, float vol, bool debug_mode) {
    SteamHelper steamHelper;

    // ================== 网络层优化 ==================
    svr->set_tcp_nodelay(true);  // 禁用Nagle算法，减少延迟

    // 设置工作线程池
    const size_t workers = std::max<size_t>(4, std::thread::hardware_concurrency() * 2);
    svr->new_task_queue = []() -> httplib::TaskQueue* {
        auto n = std::thread::hardware_concurrency();
        return new httplib::ThreadPool(n * 2);
        };
    svr->Post("/", [debug_mode, match, vol, &steamHelper](const httplib::Request& req, httplib::Response& res) {
        // ================== 热点字段预提取 ==================
        std::string phase = FastExtractPhase(req.body);

        // 阶段快速过滤 - 跳过无效请求
        if (phase != "live" && phase != "over" && phase != "gameover") {
            if (debug_mode) {
                std::cout << "[FAST SKIP] Invalid phase: "
                    << (phase.empty() ? "<empty>" : phase) << "\n";
            }
            res.set_content("IGNORED", "text/plain");
            return;
        }

        // ================== SIMD加速JSON解析 ==================
        if (debug_mode) {
            std::cout << "[RAW JSON RECEIVED]:\n" << req.body << "\n";
        }

         steamid_in_list = 0;

        // 使用SIMD加速的JSON解析器
        simdjson::dom::parser parser;
        simdjson::dom::element doc;
        auto error = parser.parse(req.body).get(doc);

        if (error) {
            if (debug_mode) {
                std::cerr << "[JSON ERROR] Parse failed: " << error << std::endl;
            }
            res.set_content("JSON PARSE ERROR", "text/plain");
            return;
        }

        // ================== 从SIMD JSON提取数据 ==================
        // 确保阶段一致性
        if (auto round = doc["round"]; round.is_object()) {
            if (auto phase_val = round["phase"]; phase_val.is_string()) {
                phase = std::string(phase_val.get_string().value());
            }
        }

        auto player = doc["player"];
        auto state = player["state"];

        int health = 100;
        int round_kills = 0;
        std::string map_mode = "competitive";
        std::string player_steamid = "";

        if (state.is_object()) {
            if (state["health"].is_int64()) health = int(state["health"].get_int64().value());
            if (state["round_kills"].is_int64()) round_kills = int(state["round_kills"].get_int64().value());
        }

        if (auto map = doc["map"]; map.is_object()) {
            if (map["mode"].is_string()) map_mode = std::string(map["mode"].get_string().value());
        }

        if (player.is_object() && player["steamid"].is_string()) {
            player_steamid = std::string(player["steamid"].get_string().value());
        }

        // ================== 以下代码保持不变 ==================
        if (debug_mode == 1) {
            std::cout << "\n你的steamid是" << player_steamid << endl;
        }

        std::unordered_set<std::string> steam64IDsSet = steamHelper.ConvertAllToSteam64IDs();
        if (steam64IDsSet.find(player_steamid) != steam64IDsSet.end()) {
            std::cout << "[Debug] Match found: Yes" << std::endl;
            steamid_in_list = 1;
        }
        else {
            std::cout << "[Debug] Match found: No" << std::endl;
        }

        bool isMenu = false;
        if (player.is_object() && player["activity"].is_string()) {
            isMenu = (std::string(player["activity"].get_string().value()) == "menu");
        }

        bool onlyHasPlayerAndProvider = true;
        for (auto field : doc.get_object()) {
            std::string_view key = field.key;
            if (key != "player" && key != "provider") {
                onlyHasPlayerAndProvider = false;
                break;
            }
        }

        if (isMenu && onlyHasPlayerAndProvider) {
            StartMenuLoop(vol);
        }
        else {
            // StopMenuLoop();
        }

        using Clock = std::chrono::high_resolution_clock;
        auto t0 = Clock::now();
        auto t1 = Clock::now();
        auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cout << "[PERF] json::parse took " << dur << " ms\n";

        // 玩家阵营
        if (player.is_object() && player["team"].is_string()) {
            player_team = std::string(player["team"].get_string().value());
        }

    
        // MVP 数
        int current_mvps = 0;
        if (player.is_object()) {
            auto match_stats = player["match_stats"];
            if (match_stats.is_object() && match_stats["mvps"].is_int64()) {
                if (steamid_in_list == 1) {
                    current_mvps = int(match_stats["mvps"].get_int64().value());
                }
            }
        }

        // 胜负字段 - 修复2: 统一检测方法
        bool has_win_team = false;
        std::string win_team;
        if (auto round = doc["round"]; round.is_object() && round["win_team"].is_string()) {
            has_win_team = true;
            win_team = std::string(round["win_team"].get_string().value());
        }
        else if (auto added = doc["added"]; added.is_object()) {
            auto added_round = added["round"];
            if (added_round.is_object() && added_round["win_team"].is_string()) {
                has_win_team = true;
                win_team = std::string(added_round["win_team"].get_string().value());
            }
        }

        // 炸弹状态
        std::string bomb_state;
        if (auto round = doc["round"]; round.is_object()) {
            auto bomb = round["bomb"];
            if (bomb.is_object() && bomb["state"].is_string()) {
                bomb_state = std::string(bomb["state"].get_string().value());
            }
        }

        bool mvp_pushed = false;  // 本次是否已推送 MVP 音效
        static int mvps_at_round_start = 0; // 新增：保存回合开始时的MVP数
        static bool mvp_pushed_this_round = false; // 新增：避免一个回合重复推送

        // ✅ 五杀音效推送（优先级最高）
        if (round_kills == 5 && !dead_muted) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            kills_queue.push(5);
            if (debug_mode)
                std::cout << "[KILL] Five kill pushed\n";
        }

        // 阶段切换逻辑
        if (phase != last_phase) {
            if (debug_mode)
                std::cout << "[PHASE] " << last_phase << " → " << phase << "\n";


            // 替换为：
            if (phase == "freezetime" && last_phase != "freezetime") {
                // 检查炸弹音效是否正在播放
                int bomb_ch = bomb_channel.load();
                if (bomb_ch >= 0 && Mix_Playing(bomb_ch)) {
                    if (debug_mode) {
                        std::cout << "[BOMB] Sound is playing, waiting to finish before playing buy sound\n";
                    }

                    // 等待炸弹音效结束
                    while (bomb_ch >= 0 && Mix_Playing(bomb_ch)) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        bomb_ch = bomb_channel.load();
                    }
                }

                // 确保停止所有可能的炸弹音效
                StopBombSound();
                StopAllSounds();

                // 播放buy音效
                std::lock_guard<std::mutex> lock(queue_mutex);
                kills_queue.push(-14);
                if (debug_mode) {
                    std::cout << "[BUY TIME] Pushed -14 after bomb sound finished\n";
                }
            }

            if (phase == "live" && waiting_for_live) {
                {
                    OnLivePhaseStart(vol);
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    kills_queue.push(-13);  // 回合开始
                }
                HideShowMVP();
                gameover_pushed = false;
                deathcounts_rd = 0;
                temp_disable_ace_when_mvp = false;
                waiting_for_live = false;
                wait_for_kill_reset = false;
                mvp_played = false;
                mvp_candidate_kills = 0;
                round_death_occurred = false;
                dead_muted = false;
                last_kills = 0;
                round_started = true;

                temp_to_disable_count_mvp = false;
                bomb_planted_this_round = false;

                if (auto map = doc["map"]; map.is_object()) {
                    auto team_ct = map["team_ct"];
                    auto team_t = map["team_t"];

                    if (team_ct.is_object() && team_ct["score"].is_int64()) {
                        round_score_ct = int(team_ct["score"].get_int64().value());
                    }
                    if (team_t.is_object() && team_t["score"].is_int64()) {
                        round_score_t = int(team_t["score"].get_int64().value());
                    }
                }

                if (debug_mode)
                    std::cout << "[RESET] New live, scores: CT=" << round_score_ct
                    << ", T=" << round_score_t << "\n";

                // 仅当玩家在列表中时重置MVP计数
                // 在 live 阶段开始时，记录当前 MVP 数量
                if (steamid_in_list == 1) {
                    mvps_at_round_start = current_mvps;
                }
                mvp_pushed_this_round = false;
            }

            if (phase != "live") {
                waiting_for_live = true;
                round_started = false;
            }

            last_phase = phase;
        }

        // ✅ MVP 处理 - 修复3: 简化条件并确保在回合结束时处理
     // 检查MVP条件，只有在回合结束且MVP音效尚未推送时执行
        if (!mvp_pushed_this_round && custom_musickit && (phase == "over" || phase == "gameover")) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            bool is_mvp = false;

            // 条件1: MVP数量比回合开始时多
            if (steamid_in_list == 1 && current_mvps > mvps_at_round_start) {
                is_mvp = true;
            }
            // 条件2: 本回合击杀数 >=3 且本方获胜
            if (has_win_team && win_team == player_team && mvp_candidate_kills >= 3) {
                is_mvp = true;
            }

            if (is_mvp) {
                kills_queue.push(-2);  // MVP 音效
                if (ShowMVP) {
                    ShowMvpOverlay();
                }
                mvp_pushed_this_round = true; // 标记本回合已推送
                if (debug_mode) {
                    std::cout << "[CUSTOM KIT] MVP pushed because conditions met.\n";
                }
            }
            else if (debug_mode && steamid_in_list == 1) {
                std::cout << "[CUSTOM KIT] MVP skip conditions: "
                    << "MVP count increase: " << (current_mvps > mvps_at_round_start)
                    << ", 3+ kills & win: " << (has_win_team && win_team == player_team && mvp_candidate_kills >= 3)
                    << "\n";
            }
        }

        // ✅ 胜负音效推送，仅当未推送 MVP 音效时 - 修复4: 确保正确判断胜负
        if (!mvp_pushed_this_round && custom_musickit && has_win_team && (phase == "over" || phase == "gameover")) {
            // 直接使用已知的赢家队伍信息
            bool we_won = (win_team == player_team);
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (we_won) {
                kills_queue.push(-3);
                if (debug_mode)
                    std::cout << "[CUSTOM KIT] WIN pushed\n";
            }
            else {
                kills_queue.push(-4);
                if (debug_mode)
                    std::cout << "[CUSTOM KIT] LOSE pushed\n";
            }
        }

        if (custom_musickit && phase == "live") {
            if (auto round = doc["round"]; round.is_object()) {
                if (auto bomb = round["bomb"]; bomb.is_string()) {
                    if (bomb.get_string().value() == "planted" && !bomb_planted_this_round) {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        kills_queue.push(-12);
                        bomb_planted_this_round = true;

                        if (debug_mode)
                            std::cout << "[BOMB] Detected planting, pushed -12\n";
                    }
                }
            }
        }

        // ✅ 跳过非有效阶段
        if (phase != "live" && phase != "over") {
            if (debug_mode)
                std::cout << "[SKIP] Not in live/over phase\n";
            res.set_content("IGNORED", "text/plain");
            if (auto round = doc["round"]; round.is_object()) {
                if (auto bomb = round["bomb"]; bomb.is_object() && bomb["state"].is_string()) {
                    last_bomb_state = std::string(bomb["state"].get_string().value());
                }
            }
            return;
        }

        // ✅ 击杀/死亡逻辑
        if (map_mode == "deathmatch") {
            if (round_kills > last_kills) {
                std::lock_guard<std::mutex> lock(queue_mutex);
                kills_queue.push(-1);
                last_kills = round_kills;
                if (debug_mode)
                    std::cout << "[DM-KILL] Pushed extra for kill #" << round_kills << "\n";
            }
        }
        else {
            if (health <= 0 && !dead_muted) {
                std::lock_guard<std::mutex> lock(queue_mutex);
                kills_queue.push(-18);
                dead_muted = true;
                round_death_occurred = true;
                deathcounts_rd += 1;
                temp_to_disable_count_mvp = true;
                if (debug_mode)
                    std::cout << "[MUTED] Player is dead\n";
            }

            if (!dead_muted && round_kills > last_kills) {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (!temp_disable_ace_when_mvp) {
                    kills_queue.push(round_kills);
                }
                if (round_kills > mvp_candidate_kills) {
                    mvp_candidate_kills = round_kills;
                }
                last_kills = round_kills;
                if (debug_mode)
                    std::cout << "[KILL] Pushed " << round_kills << "\n";
            }
        }

        // ================== 闪光弹处理 ==================
#if defined(_WIN32) || defined(_WIN64)
        if (player.is_object() && state.is_object()) {
            int flashed_now = 0;
            int flashed_before = 0;

            if (state["flashed"].is_int64()) {
                flashed_now = int(state["flashed"].get_int64().value());
            }

            if (auto previously = doc["previously"]; previously.is_object()) {
                if (auto prev_player = previously["player"]; prev_player.is_object()) {
                    if (auto prev_state = prev_player["state"]; prev_state.is_object()) {
                        if (prev_state["flashed"].is_int64()) {
                            flashed_before = int(prev_state["flashed"].get_int64().value());
                        }
                    }
                }
            }

            if (flashed_now > 0 && flashed_before == 0) {
                std::cout << "[FLASH] Player is now flashed\n";
                if (custom_flashbang) gsi_playerflash();
            }
            else if (flashed_now == 0 && flashed_before > 0) {
                std::cout << "[FLASH] Flash ended\n";
                if (custom_flashbang) gsi_endplayerflash();
            }
        }
#endif

        // 更新炸弹状态
        if (auto round = doc["round"]; round.is_object()) {
            if (auto bomb = round["bomb"]; bomb.is_object() && bomb["state"].is_string()) {
                last_bomb_state = std::string(bomb["state"].get_string().value());
            }
        }

        res.set_content("OK", "text/plain");
        });

    svr->listen("127.0.0.1", 1009);
}