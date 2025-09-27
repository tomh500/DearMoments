#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <unordered_set>
#include <chrono>
#include <iostream>
#include <simdjson.h>

#include "../core/Logger.h"
#include "../core/Constants.h"
#include "AudioManager.h"

/**
 * @brief Round state encapsulation struct
 * 
 * Centralizes all round-scoped state variables to eliminate extern dependencies
 */
struct RoundState {
    bool waitingForLive = true;
    bool roundStarted = false;
    int mvpCandidateKills = 0;
    bool roundDeathOccurred = false;
    bool deadMuted = false;
    int lastKills = 0;
    bool tempDisableAceWhenMvp = false;
    bool tempToDisableCountMvp = false;
    bool bombPlantedThisRound = false;
    int deathcountsRd = 0;
    std::string lastBombState;
};

/**
 * @brief Comprehensive game state handling system for CS2 GSI
 * 
 * Extracts all game logic from GSI.cpp:
 * - Phase transition logic (freezetime, live, over, gameover)
 * - MVP detection and scoring
 * - Kill tracking and death handling  
 * - Win/lose detection
 * - Bomb state tracking
 * - Flash detection (Windows-specific)
 * - Game mode handling (competitive vs deathmatch)
 * 
 * Preserves all existing functionality and global variables for backward compatibility.
 */
class GameStateHandler {
private:
    RoundState roundState;  // Encapsulated round state instead of extern globals
    
    GameStateHandler() = default;

public:
    /**
     * @brief Meyers singleton pattern for global state access - thread-safe since C++11
     */
    static GameStateHandler& getInstance() {
        static GameStateHandler instance;
        return instance;
    }

    // Delete copy constructor and assignment operator
    GameStateHandler(const GameStateHandler&) = delete;
    GameStateHandler& operator=(const GameStateHandler&) = delete;

    /**
     * @brief Handle phase transitions and related logic
     * 
     * Extract from GSI.cpp phase transition handling:
     * - Freezetime to live transitions
     * - Bomb sound conflict resolution
     * - Buy sound queue management
     * - Round state management
     * 
     * @param newPhase Current phase from GSI data
     * @param oldPhase Previous phase stored in last_phase
     * @param doc JSON document for additional data access
     * @param vol Volume level for audio
     */
    inline void handlePhaseTransition(const std::string& newPhase, std::string& oldPhase, 
                                     const simdjson::dom::element& doc, float vol) {
        extern bool debug_mode;
        extern std::queue<int> kills_queue;
        extern std::mutex queue_mutex;
        extern bool gameover_pushed;
        extern bool mvp_pushed;
        extern int round_score_ct;
        extern int round_score_t;
        
        if (newPhase != oldPhase) {
            if (debug_mode)
                std::cout << "[PHASE] " << oldPhase << " → " << newPhase << "\n";
            LOG_INFO_STREAM("Phase transition: " << oldPhase << " → " << newPhase);

            // Freezetime transition logic
            if (newPhase == "freezetime" && oldPhase != "freezetime") {
                // 检查炸弹音效是否正在播放 - использование AudioManager вместо bomb_channel
                auto& audio = AudioManager::getInstance();
                if (audio.isBombPlaying()) {
                    if (debug_mode) {
                        std::cout << "[BOMB] Sound is playing, waiting to finish before playing buy sound\n";
                    }
                    LOG_DEBUG("Bomb sound is playing, waiting to finish before buy sound");

                    // 等待炸弹音效结束
                    while (audio.isBombPlaying()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                }

                // 确保停止所有可能的炸弹音效
                AudioManager::getInstance().stopBombSound();
                AudioManager::getInstance().stopAllSounds();

                // 播放buy音效
                std::lock_guard<std::mutex> lock(queue_mutex);
                kills_queue.push(-14);
                if (debug_mode) {
                    std::cout << "[BUY TIME] Pushed -14 after bomb sound finished\n";
                }
                LOG_INFO("Buy phase started, pushed buy sound to queue");
            }

            // Live phase transition logic
            if (newPhase == "live" && roundState.waitingForLive) {
                {
                    AudioManager::getInstance().startLivePhase(vol);
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    kills_queue.push(-13);  // 回合开始
                }
                LOG_INFO("Live phase started, round beginning");
                extern void HideShowMVP();
                HideShowMVP();
                gameover_pushed = false;
                roundState.deathcountsRd = 0;
                roundState.tempDisableAceWhenMvp = false;
                roundState.waitingForLive = false;
                extern bool wait_for_kill_reset;
                wait_for_kill_reset = false;
                mvp_pushed = false;
                roundState.mvpCandidateKills = 0;
                roundState.roundDeathOccurred = false;
                roundState.deadMuted = false;
                roundState.lastKills = 0;
                roundState.roundStarted = true;

                roundState.tempToDisableCountMvp = false;
                roundState.bombPlantedThisRound = false;

                // Update team scores
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
            }

            if (newPhase != "live") {
                roundState.waitingForLive = true;
                roundState.roundStarted = false;
            }

            oldPhase = newPhase;
        }
    }

    /**
     * @brief Process MVP logic and detection
     * 
     * Extract from GSI.cpp MVP processing:
     * - MVP count comparison from round start
     * - 3+ kills + team win conditions
     * - Custom musickit integration
     * - Round-based MVP tracking
     * 
     * @param currentMVPs Current MVP count from GSI data
     * @param roundKills Round kills for MVP candidate logic
     * @param hasWinTeam Whether win team is available
     * @param winTeam Winning team name
     * @param playerTeam Player's team
     * @param phase Current game phase
     */
    inline void processMVPLogic(int currentMVPs, int roundKills, bool hasWinTeam, 
                               const std::string& winTeam, const std::string& playerTeam, 
                               const std::string& phase) {
        extern bool custom_musickit;
        extern int steamid_in_list;
        extern bool debug_mode;
        extern std::queue<int> kills_queue;
        extern std::mutex queue_mutex;
        extern bool ShowMVP;
        
        static int mvps_at_round_start = 0;
        extern bool mvp_pushed;  // Use shared global flag instead of function-local

        // Reset MVP tracking at live phase start
        if (phase == "live" && steamid_in_list == 1) {
            mvps_at_round_start = currentMVPs;
            mvp_pushed = false;
        }

        // MVP processing - only at round end
        if (!mvp_pushed && custom_musickit && (phase == "over" || phase == "gameover")) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            bool is_mvp = false;

            // 条件1: MVP数量比回合开始时多
            if (steamid_in_list == 1 && currentMVPs > mvps_at_round_start) {
                is_mvp = true;
            }
            // 条件2: 本回合击杀数 >=3 且本方获胜
            if (hasWinTeam && winTeam == playerTeam && roundState.mvpCandidateKills >= 3) {
                is_mvp = true;
            }

            if (is_mvp) {
                kills_queue.push(-2);  // MVP 音效
                if (ShowMVP) {
                    extern void ShowMvpOverlay();
                    ShowMvpOverlay();
                }
                mvp_pushed = true; // 标记本回合已推送
                if (debug_mode) {
                    std::cout << "[CUSTOM KIT] MVP pushed because conditions met.\n";
                }
                LOG_INFO("MVP achieved, pushed MVP sound to queue");
            }
            else if (debug_mode && steamid_in_list == 1) {
                std::cout << "[CUSTOM KIT] MVP skip conditions: "
                    << "MVP count increase: " << (currentMVPs > mvps_at_round_start)
                    << ", 3+ kills & win: " << (hasWinTeam && winTeam == playerTeam && roundState.mvpCandidateKills >= 3)
                    << "\n";
            }
        }
    }

    /**
     * @brief Process kill and death logic
     * 
     * Extract from GSI.cpp kill/death processing:
     * - Competitive vs deathmatch modes
     * - Five-kill detection
     * - Death muting and round death tracking
     * - MVP candidate kill tracking
     * 
     * @param roundKills Current round kills
     * @param health Player health
     * @param mapMode Game mode (competitive/deathmatch)
     * @param phase Current game phase
     */
    inline void processKillLogic(int roundKills, int health, const std::string& mapMode, 
                                const std::string& phase) {
        extern bool debug_mode;
        extern std::queue<int> kills_queue;
        extern std::mutex queue_mutex;

        // Five-kill detection (highest priority)
        if (roundKills == 5 && !roundState.deadMuted) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            kills_queue.push(5);
            if (debug_mode)
                std::cout << "[KILL] Five kill pushed\n";
        }

        // Different logic for deathmatch vs competitive
        if (mapMode == "deathmatch") {
            if (roundKills > roundState.lastKills) {
                std::lock_guard<std::mutex> lock(queue_mutex);
                kills_queue.push(-1);
                roundState.lastKills = roundKills;
                if (debug_mode)
                    std::cout << "[DM-KILL] Pushed extra for kill #" << roundKills << "\n";
            }
        }
        else {
            // Death handling
            if (health <= 0 && !roundState.deadMuted) {
                std::lock_guard<std::mutex> lock(queue_mutex);
                kills_queue.push(-18);
                roundState.deadMuted = true;
                roundState.roundDeathOccurred = true;
                roundState.deathcountsRd += 1;
                roundState.tempToDisableCountMvp = true;
                if (debug_mode)
                    std::cout << "[MUTED] Player is dead\n";
            }

            // Kill tracking
            if (!roundState.deadMuted && roundKills > roundState.lastKills) {
                std::lock_guard<std::mutex> lock(queue_mutex);
                if (!roundState.tempDisableAceWhenMvp) {
                    kills_queue.push(roundKills);
                }
                if (roundKills > roundState.mvpCandidateKills) {
                    roundState.mvpCandidateKills = roundKills;
                }
                roundState.lastKills = roundKills;
                if (debug_mode)
                    std::cout << "[KILL] Pushed " << roundKills << "\n";
            }
        }
    }

    /**
     * @brief Process win/lose logic
     * 
     * Extract from GSI.cpp win/lose processing:
     * - Team-based win detection
     * - Integration with MVP logic to avoid conflicts
     * - Custom musickit win/lose sound triggers
     * 
     * @param hasWinTeam Whether win team data is available
     * @param winTeam Winning team name
     * @param playerTeam Player's team
     * @param phase Current game phase
     */
    inline void processWinLoseLogic(bool hasWinTeam, const std::string& winTeam, 
                                   const std::string& playerTeam, const std::string& phase) {
        extern bool custom_musickit;
        extern bool debug_mode;
        extern std::queue<int> kills_queue;
        extern std::mutex queue_mutex;
        
        extern bool mvp_pushed;  // Use shared global flag

        // Win/lose processing - only when MVP not pushed
        if (!mvp_pushed && custom_musickit && hasWinTeam && (phase == "over" || phase == "gameover")) {
            // 直接使用已知的赢家队伍信息
            bool we_won = (winTeam == playerTeam);
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (we_won) {
                kills_queue.push(-3);
                if (debug_mode)
                    std::cout << "[CUSTOM KIT] WIN pushed\n";
                LOG_INFO("Round won, pushed win sound to queue");
            }
            else {
                kills_queue.push(-4);
                if (debug_mode)
                    std::cout << "[CUSTOM KIT] LOSE pushed\n";
                LOG_INFO("Round lost, pushed lose sound to queue");
            }
        }
    }

    /**
     * @brief Process bomb state logic
     * 
     * Extract from GSI.cpp bomb processing:
     * - Bomb planted detection
     * - Sound trigger integration
     * - Bomb state tracking across phases
     * 
     * @param doc JSON document for bomb state access
     * @param phase Current game phase
     */
    inline void processBombLogic(const simdjson::dom::element& doc, const std::string& phase) {
        extern bool custom_musickit;
        extern bool debug_mode;
        extern std::queue<int> kills_queue;
        extern std::mutex queue_mutex;

        if (custom_musickit && phase == "live") {
            if (auto round = doc["round"]; round.is_object()) {
                if (auto bomb = round["bomb"]; bomb.is_string()) {
                    if (bomb.get_string().value() == "planted" && !roundState.bombPlantedThisRound) {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        kills_queue.push(-12);
                        roundState.bombPlantedThisRound = true;

                        if (debug_mode)
                            std::cout << "[BOMB] Detected planting, pushed -12\n";
                        LOG_INFO("Bomb planted, pushed bomb sound to queue");
                    }
                }
            }
        }

        // Update bomb state tracking
        if (auto round = doc["round"]; round.is_object()) {
            if (auto bomb = round["bomb"]; bomb.is_object() && bomb["state"].is_string()) {
                roundState.lastBombState = std::string(bomb["state"].get_string().value());
            }
        }
    }

    /**
     * @brief Process flash logic (Windows-specific)
     * 
     * Extract from GSI.cpp flash processing:
     * - Windows-specific flash detection
     * - Integration with custom flashbang settings
     * - Flash start/end event handling
     * 
     * @param doc JSON document for flash state access
     */
    inline void processFlashLogic(const simdjson::dom::element& doc) {
#if defined(_WIN32) || defined(_WIN64)
        extern bool custom_flashbang;
        extern void gsi_playerflash();
        extern void gsi_endplayerflash();

        auto player = doc["player"];
        auto state = player["state"];

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
    }

    /**
     * @brief Main game state processing orchestrator
     * 
     * Coordinates all game state processing components:
     * - Phase transitions
     * - MVP detection
     * - Kill/death tracking
     * - Win/lose detection
     * - Bomb state management
     * - Flash detection
     * 
     * @param doc JSON document with game state data
     * @param vol Volume level for audio operations
     */
    inline void processGameState(const simdjson::dom::element& doc, float vol) {
        extern std::string last_phase;
        extern std::string player_team;
        extern int steamid_in_list;
        extern bool debug_mode;

        // Extract basic game state data
        std::string phase;
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

        if (state.is_object()) {
            if (state["health"].is_int64()) health = int(state["health"].get_int64().value());
            if (state["round_kills"].is_int64()) round_kills = int(state["round_kills"].get_int64().value());
        }

        if (auto map = doc["map"]; map.is_object()) {
            if (map["mode"].is_string()) map_mode = std::string(map["mode"].get_string().value());
        }

        // Player team
        if (player.is_object() && player["team"].is_string()) {
            player_team = std::string(player["team"].get_string().value());
        }

        // MVP count
        int current_mvps = 0;
        if (player.is_object()) {
            auto match_stats = player["match_stats"];
            if (match_stats.is_object() && match_stats["mvps"].is_int64()) {
                if (steamid_in_list == 1) {
                    current_mvps = int(match_stats["mvps"].get_int64().value());
                }
            }
        }

        // Win team detection
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

        // Process all game state components
        handlePhaseTransition(phase, last_phase, doc, vol);
        processMVPLogic(current_mvps, round_kills, has_win_team, win_team, player_team, phase);
        processKillLogic(round_kills, health, map_mode, phase);
        processWinLoseLogic(has_win_team, win_team, player_team, phase);
        processBombLogic(doc, phase);
        processFlashLogic(doc);
    }
};
